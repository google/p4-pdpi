// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "p4_pdpi/ir.h"

#include <google/protobuf/util/message_differencer.h>

#include <sstream>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "gutil/collections.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4_pdpi/utils/ir.h"

namespace pdpi {

using ::google::protobuf::util::MessageDifferencer;
using ::gutil::InvalidArgumentErrorBuilder;
using ::p4::config::v1::MatchField;
using ::p4::config::v1::P4TypeInfo;
using ::pdpi::Format;
using ::pdpi::IrActionDefinition;
using ::pdpi::IrActionInvocation;
using ::pdpi::IrMatchFieldDefinition;
using ::pdpi::IrP4Info;
using ::pdpi::IrTableDefinition;

namespace {

// Helper for GetFormat that extracts the necessary info from a P4Info element.
// T could be p4::config::v1::ControllerPacketMetadata::Metadata,
// p4::config::v1::MatchField, or p4::config::v1::Action::Param (basically
// anything that has a set of annotations, a bitwidth and named type
// information).
template <typename T>
gutil::StatusOr<Format> GetFormatForP4InfoElement(const T &element,
                                                  const P4TypeInfo &type_info) {
  bool is_sdn_string = false;
  if (element.has_type_name()) {
    const auto &name = element.type_name().name();
    ASSIGN_OR_RETURN(const auto &named_type,
                     gutil::FindOrStatus(type_info.new_types(), name),
                     _ << "Type definition for \"" << name << "\" not found.");
    if (named_type.has_translated_type()) {
      if (named_type.translated_type().sdn_type_case() ==
          p4::config::v1::P4NewTypeTranslation::kSdnString) {
        is_sdn_string = true;
      }
    }
  }
  std::vector<std::string> annotations;
  for (const auto &annotation : element.annotations()) {
    annotations.push_back(annotation);
  }
  return GetFormat(annotations, element.bitwidth(), is_sdn_string);
}

// Add a single packet-io metadata to the IR.
absl::Status ProcessPacketIoMetadataDefinition(
    const p4::config::v1::ControllerPacketMetadata &data,
    google::protobuf::Map<uint32_t, IrPacketIoMetadataDefinition> *by_id,
    google::protobuf::Map<std::string, IrPacketIoMetadataDefinition> *by_name,
    const P4TypeInfo &type_info) {
  const std::string &kind = data.preamble().name();
  if (!by_id->empty()) {
    // Only checking by_id, since by_id->size() == by_name->size()
    return gutil::InvalidArgumentErrorBuilder()
           << "Found duplicate \"" << kind << "\" controller packet metadata.";
  }
  for (const auto &metadata : data.metadata()) {
    IrPacketIoMetadataDefinition ir_metadata;
    *ir_metadata.mutable_metadata() = metadata;
    ASSIGN_OR_RETURN(const auto &format,
                     GetFormatForP4InfoElement(metadata, type_info));
    ir_metadata.set_format(format);
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        by_id, metadata.id(), ir_metadata,
        absl::StrCat("Found several \"", kind,
                     "\" metadata with the same ID: ", metadata.id(), ".")));
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        by_name, metadata.name(), ir_metadata,
        absl::StrCat("Found several \"", kind,
                     "\" metadata with the same name: ", metadata.name(),
                     ".")));
  }
  return absl::OkStatus();
}

// Searches for an annotation with the given name and extract a single uint32_t
// number from the argument. Fails if the annotation appears multiple times.
gutil::StatusOr<uint32_t> GetNumberInAnnotation(
    const google::protobuf::RepeatedPtrField<std::string> &annotations,
    const std::string &annotation_name) {
  absl::optional<uint32_t> result;
  for (const std::string &annotation : annotations) {
    absl::string_view view = annotation;
    if (absl::ConsumePrefix(&view, absl::StrCat("@", annotation_name, "("))) {
      if (result.has_value()) {
        return InvalidArgumentErrorBuilder()
               << "Cannot have multiple annotations with the name \""
               << annotation_name << "\".";
      }
      const std::string number = std::string(absl::StripSuffix(view, ")"));
      for (const char c : number) {
        if (!isdigit(c)) {
          return InvalidArgumentErrorBuilder()
                 << "Expected the argument to @" << annotation_name
                 << " to be a number, but found non-number character.";
        }
      }
      result = std::stoi(number);
    }
  }
  if (!result.has_value()) {
    return InvalidArgumentErrorBuilder()
           << "No annotation found with name \"" << annotation_name << "\".";
  }
  return result.value();
}

int GetNumMandatoryMatches(const IrTableDefinition &table) {
  int mandatory_matches = 0;
  for (const auto &iter : table.match_fields_by_name()) {
    if (iter.second.match_field().match_type() == MatchField::EXACT) {
      mandatory_matches += 1;
    }
  }
  return mandatory_matches;
}

absl::Status ValidateMatchFieldDefinition(const IrMatchFieldDefinition &match) {
  switch (match.match_field().match_type()) {
    case p4::config::v1::MatchField::LPM:
    case p4::config::v1::MatchField::TERNARY:
      if (match.format() == Format::STRING) {
        return InvalidArgumentErrorBuilder()
               << "Only EXACT and OPTIONAL match fields can use "
                  "Format::STRING: "
               << match.match_field().ShortDebugString() << ".";
      }
    case p4::config::v1::MatchField::EXACT:
    case p4::config::v1::MatchField::OPTIONAL:
      break;
    default:
      return InvalidArgumentErrorBuilder()
             << "Match field match type not supported: "
             << match.match_field().ShortDebugString() << ".";
  }
  return absl::OkStatus();
}

}  // namespace

gutil::StatusOr<IrP4Info> CreateIrP4Info(
    const p4::config::v1::P4Info &p4_info) {
  IrP4Info info;
  const P4TypeInfo &type_info = p4_info.type_info();

  // Translate all action definitions to IR.
  for (const auto &action : p4_info.actions()) {
    IrActionDefinition ir_action;
    *ir_action.mutable_preamble() = action.preamble();
    for (const auto &param : action.params()) {
      IrActionDefinition::IrActionParamDefinition ir_param;
      *ir_param.mutable_param() = param;
      ASSIGN_OR_RETURN(const auto &format,
                       GetFormatForP4InfoElement(param, type_info));
      ir_param.set_format(format);
      RETURN_IF_ERROR(gutil::InsertIfUnique(
          ir_action.mutable_params_by_id(), param.id(), ir_param,
          absl::StrCat("Found several parameters with the same ID ", param.id(),
                       " for action ", action.preamble().alias(), ".")));
      RETURN_IF_ERROR(gutil::InsertIfUnique(
          ir_action.mutable_params_by_name(), param.name(), ir_param,
          absl::StrCat("Found several parameters with the same name \"",
                       param.name(), "\" for action \"",
                       action.preamble().alias(), "\".")));
    }
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        info.mutable_actions_by_id(), action.preamble().id(), ir_action,
        absl::StrCat("Found several actions with the same ID: ",
                     action.preamble().id(), ".")));
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        info.mutable_actions_by_name(), action.preamble().alias(), ir_action,
        absl::StrCat("Found several actions with the same name: ",
                     action.preamble().name(), ".")));
  }

  // Translate all table definitions to IR.
  for (const auto &table : p4_info.tables()) {
    IrTableDefinition ir_table_definition;
    uint32_t table_id = table.preamble().id();
    *ir_table_definition.mutable_preamble() = table.preamble();
    for (const auto match_field : table.match_fields()) {
      IrMatchFieldDefinition ir_match_definition;
      *ir_match_definition.mutable_match_field() = match_field;
      ASSIGN_OR_RETURN(const auto &format,
                       GetFormatForP4InfoElement(match_field, type_info));
      ir_match_definition.set_format(format);
      RETURN_IF_ERROR(ValidateMatchFieldDefinition(ir_match_definition))
          << "Table " << table.preamble().alias()
          << " has invalid match field.";

      RETURN_IF_ERROR(gutil::InsertIfUnique(
          ir_table_definition.mutable_match_fields_by_id(), match_field.id(),
          ir_match_definition,
          absl::StrCat("Found several match fields with the same ID ",
                       match_field.id(), " in table \"",
                       table.preamble().alias(), "\".")));
      RETURN_IF_ERROR(gutil::InsertIfUnique(
          ir_table_definition.mutable_match_fields_by_name(),
          match_field.name(), ir_match_definition,
          absl::StrCat("Found several match fields with the same name \"",
                       match_field.name(), "\" in table \"",
                       table.preamble().alias(), "\".")));
    }

    // Is WCMP table?
    const bool is_wcmp = table.implementation_id() != 0;
    const bool has_oneshot = absl::c_any_of(
        table.preamble().annotations(),
        [](const std::string &annotation) { return annotation == "@oneshot"; });
    if (is_wcmp != has_oneshot) {
      return InvalidArgumentErrorBuilder()
             << "A WCMP table must have a @oneshot annotation, but \""
             << table.preamble().alias()
             << "\" is not valid. is_wcmp = " << is_wcmp
             << ", has_oneshot = " << has_oneshot << ".";
    }
    if (is_wcmp) {
      ir_table_definition.set_is_wcmp(true);
      ASSIGN_OR_RETURN(
          const uint32_t weight_proto_id,
          GetNumberInAnnotation(table.preamble().annotations(),
                                "weight_proto_id"),
          _ << "WCMP table \"" << table.preamble().alias()
            << "\" does not have a valid @weight_proto_id annotation.");
      ir_table_definition.set_weight_proto_id(weight_proto_id);
    }

    for (const auto &action_ref : table.action_refs()) {
      IrActionReference ir_action_reference;
      *ir_action_reference.mutable_ref() = action_ref;
      // Make sure the action is defined
      ASSIGN_OR_RETURN(
          *ir_action_reference.mutable_action(),
          gutil::FindOrStatus(info.actions_by_id(), action_ref.id()),
          _ << "Missing definition for action with id " << action_ref.id()
            << ".");
      uint32_t proto_id = 0;
      if (action_ref.scope() != p4::config::v1::ActionRef::DEFAULT_ONLY) {
        ASSIGN_OR_RETURN(
            proto_id,
            GetNumberInAnnotation(action_ref.annotations(), "proto_id"),
            _ << "Action \"" << ir_action_reference.action().preamble().name()
              << "\" in table \"" << table.preamble().alias()
              << "\" does not have a valid @proto_id annotation.");
      }
      ir_action_reference.set_proto_id(proto_id);
      *ir_table_definition.add_actions() = ir_action_reference;
    }
    ir_table_definition.set_size(table.size());
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        info.mutable_tables_by_id(), table_id, ir_table_definition,
        absl::StrCat("Found several tables with the same ID ",
                     table.preamble().id(), ".")));
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        info.mutable_tables_by_name(), table.preamble().alias(),
        ir_table_definition,
        absl::StrCat("Found several tables with the same name \"",
                     table.preamble().alias(), "\".")));
  }

  // Validate and translate the packet-io metadata
  for (const auto &metadata : p4_info.controller_packet_metadata()) {
    const std::string &kind = metadata.preamble().name();
    if (kind == "packet_out") {
      RETURN_IF_ERROR(ProcessPacketIoMetadataDefinition(
          metadata, info.mutable_packet_out_metadata_by_id(),
          info.mutable_packet_out_metadata_by_name(), type_info));
    } else if (kind == "packet_in") {
      RETURN_IF_ERROR(ProcessPacketIoMetadataDefinition(
          metadata, info.mutable_packet_in_metadata_by_id(),
          info.mutable_packet_in_metadata_by_name(), type_info));
    } else {
      return gutil::InvalidArgumentErrorBuilder()
             << "Unknown controller packet metadata: " << kind
             << ". Only packet_in and packet_out are supported.";
    }
  }

  return info;
}

namespace {

// Verifies the contents of the PI representation and translates to the IR
// message
gutil::StatusOr<IrMatch> PiMatchFieldToIr(
    const IrP4Info &info, const IrMatchFieldDefinition &ir_match_definition,
    const p4::v1::FieldMatch &pi_match) {
  IrMatch match_entry;
  const MatchField &match_field = ir_match_definition.match_field();
  uint32_t bitwidth = match_field.bitwidth();

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!pi_match.has_exact()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected exact match type in PI.";
      }

      match_entry.set_name(match_field.name());
      ASSIGN_OR_RETURN(
          *match_entry.mutable_exact(),
          ArbitraryByteStringToIrValue(ir_match_definition.format(), bitwidth,
                                       pi_match.exact().value()));
      break;
    }
    case MatchField::LPM: {
      if (!pi_match.has_lpm()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected LPM match type in PI.";
      }

      uint32_t prefix_len = pi_match.lpm().prefix_len();
      if (prefix_len > bitwidth) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Prefix length " << prefix_len << " is greater than bitwidth "
               << bitwidth << " in LPM.";
      }

      if (prefix_len == 0) {
        return gutil::InvalidArgumentErrorBuilder()
               << "A wild-card LPM match (i.e., prefix length of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_name(match_field.name());
      ASSIGN_OR_RETURN(const auto mask, PrefixLenToMask(prefix_len, bitwidth));
      ASSIGN_OR_RETURN(const auto value, ArbitraryToNormalizedByteString(
                                             pi_match.lpm().value(), bitwidth));
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return gutil::InvalidArgumentErrorBuilder()
               << "LPM value has masked bits that are set. Value: \""
               << absl::CEscape(value) << "\" Prefix Length: " << prefix_len;
      }
      match_entry.mutable_lpm()->set_prefix_length(prefix_len);
      ASSIGN_OR_RETURN(*match_entry.mutable_lpm()->mutable_value(),
                       ArbitraryByteStringToIrValue(
                           ir_match_definition.format(), bitwidth, value));
      break;
    }
    case MatchField::TERNARY: {
      if (!pi_match.has_ternary()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected ternary match type in PI.";
      }

      ASSIGN_OR_RETURN(const auto &value,
                       ArbitraryToNormalizedByteString(
                           pi_match.ternary().value(), bitwidth));
      ASSIGN_OR_RETURN(
          const auto &mask,
          ArbitraryToNormalizedByteString(pi_match.ternary().mask(), bitwidth));

      if (IsAllZeros(mask)) {
        return gutil::InvalidArgumentErrorBuilder()
               << "A wild-card ternary match (i.e., mask of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_name(match_field.name());
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Ternary value has masked bits that are set.\nValue: "
               << absl::CEscape(value) << " Mask: " << absl::CEscape(mask);
      }
      ASSIGN_OR_RETURN(*match_entry.mutable_ternary()->mutable_value(),
                       ArbitraryByteStringToIrValue(
                           ir_match_definition.format(), bitwidth, value));
      ASSIGN_OR_RETURN(*match_entry.mutable_ternary()->mutable_mask(),
                       ArbitraryByteStringToIrValue(
                           ir_match_definition.format(), bitwidth, mask));
      break;
    }
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unsupported match type \""
             << MatchField_MatchType_Name(match_field.match_type())
             << "\" in \"" << match_entry.name() << "\".";
  }
  return match_entry;
}

// Verifies the contents of the IR representation and translates to the PI
// message
gutil::StatusOr<p4::v1::FieldMatch> IrMatchFieldToPi(
    const IrP4Info &info, const IrMatchFieldDefinition &ir_match_definition,
    const IrMatch &ir_match) {
  p4::v1::FieldMatch match_entry;
  const MatchField &match_field = ir_match_definition.match_field();
  uint32_t bitwidth = match_field.bitwidth();

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!ir_match.has_exact()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected exact match type in IR table entry.";
      }

      match_entry.set_field_id(match_field.id());
      RETURN_IF_ERROR(ValidateIrValueFormat(ir_match.exact(),
                                            ir_match_definition.format()));
      ASSIGN_OR_RETURN(
          const auto &value,
          IrValueToNormalizedByteString(
              ir_match.exact(), ir_match_definition.match_field().bitwidth()));
      match_entry.mutable_exact()->set_value(
          NormalizedToCanonicalByteString(value));
      break;
    }
    case MatchField::LPM: {
      if (!ir_match.has_lpm()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected LPM match type in IR table entry.";
      }

      uint32_t prefix_len = ir_match.lpm().prefix_length();
      if (prefix_len > bitwidth) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Prefix length " << prefix_len << " is greater than bitwidth "
               << bitwidth << " in LPM.";
      }

      RETURN_IF_ERROR(ValidateIrValueFormat(ir_match.lpm().value(),
                                            ir_match_definition.format()));
      ASSIGN_OR_RETURN(const auto &value,
                       IrValueToNormalizedByteString(
                           ir_match.lpm().value(),
                           ir_match_definition.match_field().bitwidth()));
      if (prefix_len == 0) {
        return gutil::InvalidArgumentErrorBuilder()
               << "A wild-card LPM match (i.e., prefix length of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_field_id(match_field.id());
      ASSIGN_OR_RETURN(const auto mask, PrefixLenToMask(prefix_len, bitwidth));
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return gutil::InvalidArgumentErrorBuilder()
               << "LPM value has masked bits that are set.\nValue: "
               << ir_match.lpm().value().DebugString()
               << "Prefix Length: " << prefix_len;
      }
      match_entry.mutable_lpm()->set_prefix_len(prefix_len);
      match_entry.mutable_lpm()->set_value(
          NormalizedToCanonicalByteString(value));
      break;
    }
    case MatchField::TERNARY: {
      if (!ir_match.has_ternary()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected ternary match type in IR table entry.";
      }

      RETURN_IF_ERROR(ValidateIrValueFormat(ir_match.ternary().value(),
                                            ir_match_definition.format()));
      RETURN_IF_ERROR(ValidateIrValueFormat(ir_match.ternary().mask(),
                                            ir_match_definition.format()));
      ASSIGN_OR_RETURN(const auto &value,
                       IrValueToNormalizedByteString(
                           ir_match.ternary().value(),
                           ir_match_definition.match_field().bitwidth()));
      ASSIGN_OR_RETURN(const auto &mask,
                       IrValueToNormalizedByteString(
                           ir_match.ternary().mask(),
                           ir_match_definition.match_field().bitwidth()));
      if (IsAllZeros(mask)) {
        return gutil::InvalidArgumentErrorBuilder()
               << "A wild-card ternary match (i.e., mask of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_field_id(match_field.id());
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Ternary value has masked bits that are set.\nValue: "
               << ir_match.ternary().value().DebugString()
               << "Mask : " << ir_match.ternary().mask().DebugString();
      }
      match_entry.mutable_ternary()->set_value(
          NormalizedToCanonicalByteString(value));
      match_entry.mutable_ternary()->set_mask(
          NormalizedToCanonicalByteString(mask));
      break;
    }
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unsupported match type \""
             << MatchField_MatchType_Name(match_field.match_type()) << "\" in "
             << "match field with id " << match_entry.field_id() << ".";
  }
  return match_entry;
}

// Translates the action invocation from its PI form to IR.
gutil::StatusOr<IrActionInvocation> PiActionInvocationToIr(
    const IrP4Info &info, const p4::v1::TableAction &pi_table_action,
    const google::protobuf::RepeatedPtrField<IrActionReference>
        &valid_actions) {
  IrActionInvocation action_entry;
  switch (pi_table_action.type_case()) {
    case p4::v1::TableAction::kAction: {
      const auto pi_action = pi_table_action.action();
      uint32_t action_id = pi_action.action_id();

      ASSIGN_OR_RETURN(
          const auto &ir_action_definition,
          gutil::FindOrStatus(info.actions_by_id(), action_id),
          _ << "Action ID " << action_id << " does not exist in P4Info.");

      if (absl::c_find_if(valid_actions,
                          [action_id](const IrActionReference &action) {
                            return action.action().preamble().id() == action_id;
                          }) == valid_actions.end()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Action ID " << action_id
               << " is not a valid action for this table.";
      }

      int action_params_size = ir_action_definition.params_by_id().size();
      if (action_params_size != pi_action.params().size()) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected " << action_params_size << " parameters, but got "
               << pi_action.params().size() << " instead in action with ID "
               << action_id << ".";
      }
      action_entry.set_name(ir_action_definition.preamble().alias());
      absl::flat_hash_set<uint32_t> used_params;
      for (const auto &param : pi_action.params()) {
        RETURN_IF_ERROR(gutil::InsertIfUnique(
            used_params, param.param_id(),
            absl::StrCat("Duplicate param field found with ID ",
                         param.param_id(), ".")));

        ASSIGN_OR_RETURN(
            const auto &ir_param_definition,
            gutil::FindOrStatus(ir_action_definition.params_by_id(),
                                param.param_id()),
            _ << "Unable to find param ID " << param.param_id()
              << " in action with ID " << action_id);
        IrActionInvocation::IrActionParam *param_entry =
            action_entry.add_params();
        param_entry->set_name(ir_param_definition.param().name());
        ASSIGN_OR_RETURN(
            *param_entry->mutable_value(),
            ArbitraryByteStringToIrValue(ir_param_definition.format(),
                                         ir_param_definition.param().bitwidth(),
                                         param.value()));
      }
      break;
    }
    default:
      return gutil::UnimplementedErrorBuilder()
             << "Unsupported action type: " << pi_table_action.type_case();
  }
  return action_entry;
}

// Translates the action invocation from its IR form to PI.
gutil::StatusOr<p4::v1::TableAction> IrActionInvocationToPi(
    const IrP4Info &info, const IrActionInvocation &ir_table_action,
    const google::protobuf::RepeatedPtrField<IrActionReference>
        &valid_actions) {
  p4::v1::TableAction action_entry;
  std::string action_name = ir_table_action.name();

  ASSIGN_OR_RETURN(
      const auto &ir_action_definition,
      gutil::FindOrStatus(info.actions_by_name(), action_name),
      _ << "Action \"" << action_name << "\" does not exist in P4Info.");

  if (absl::c_find_if(
          valid_actions, [action_name](const IrActionReference &action) {
            return action.action().preamble().alias() == action_name;
          }) == valid_actions.end()) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Action \"" << action_name
           << "\" is not a valid action for this table.";
  }

  int action_params_size = ir_action_definition.params_by_name().size();
  if (action_params_size != ir_table_action.params().size()) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected " << action_params_size << " parameters, but got "
           << ir_table_action.params().size() << " instead in action \""
           << action_name << "\".";
  }

  p4::v1::Action *action = action_entry.mutable_action();
  action->set_action_id(ir_action_definition.preamble().id());
  absl::flat_hash_set<std::string> used_params;
  for (const auto &param : ir_table_action.params()) {
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_params, param.name(),
        absl::StrCat("Duplicate param field found with name \"", param.name(),
                     "\".")));

    ASSIGN_OR_RETURN(const auto &ir_param_definition,
                     gutil::FindOrStatus(ir_action_definition.params_by_name(),
                                         param.name()),
                     _ << "Unable to find param \"" << param.name()
                       << "\" in action \"" << action_name << "\".");
    p4::v1::Action_Param *param_entry = action->add_params();
    param_entry->set_param_id(ir_param_definition.param().id());
    RETURN_IF_ERROR(
        ValidateIrValueFormat(param.value(), ir_param_definition.format()));
    ASSIGN_OR_RETURN(
        const auto &value,
        IrValueToNormalizedByteString(param.value(),
                                      ir_param_definition.param().bitwidth()));
    param_entry->set_value(NormalizedToCanonicalByteString(value));
  }
  return action_entry;
}

// Generic helper that works for both packet-in and packet-out. For both, I is
// one of p4::v1::{PacketIn, PacketOut} and O is one of {IrPacketIn,
// IrPacketOut}.
template <typename I, typename O>
gutil::StatusOr<O> PiPacketIoToIr(const IrP4Info &info, const std::string &kind,
                                  const I &packet) {
  O result;
  result.set_payload(packet.payload());
  absl::flat_hash_set<uint32_t> used_metadata_ids;
  for (const auto &metadata : packet.metadata()) {
    uint32_t id = metadata.metadata_id();
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_metadata_ids, id,
        absl::StrCat("Duplicate \"", kind, "\" metadata found with ID ", id,
                     ".")));

    ASSIGN_OR_RETURN(
        const auto &metadata_definition,
        gutil::FindOrStatus(info.packet_in_metadata_by_id(), id),
        _ << kind << " metadata with ID " << id << " not defined.");
    IrPacketMetadata ir_metadata;
    ir_metadata.set_name(metadata_definition.metadata().name());
    ASSIGN_OR_RETURN(
        *ir_metadata.mutable_value(),
        ArbitraryByteStringToIrValue(metadata_definition.format(),
                                     metadata_definition.metadata().bitwidth(),
                                     metadata.value()));
    *result.add_metadata() = ir_metadata;
  }
  // Check for missing metadata
  for (const auto &item : info.packet_in_metadata_by_id()) {
    const auto &id = item.first;
    const auto &meta = item.second;
    if (!used_metadata_ids.contains(id)) {
      return gutil::InvalidArgumentErrorBuilder()
             << "\"" << kind << "\" metadata \"" << meta.metadata().name()
             << "\" with ID " << id << " is missing in P4Info.";
    }
  }

  return result;
}

template <typename I, typename O>
gutil::StatusOr<I> IrPacketIoToPi(const IrP4Info &info, const std::string &kind,
                                  const O &packet) {
  I result;
  result.set_payload(packet.payload());
  absl::flat_hash_set<std::string> used_metadata_names;
  for (const auto &metadata : packet.metadata()) {
    const std::string &name = metadata.name();
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_metadata_names, name,
        absl::StrCat("Duplicate \"", kind, "\" metadata found with name \"",
                     name, "\".")));

    ASSIGN_OR_RETURN(
        const auto &metadata_definition,
        gutil::FindOrStatus(info.packet_in_metadata_by_name(), name),
        _ << "\"" << kind << "\" metadata with name \"" << name
          << "\" not defined.");
    p4::v1::PacketMetadata pi_metadata;
    pi_metadata.set_metadata_id(metadata_definition.metadata().id());
    RETURN_IF_ERROR(
        ValidateIrValueFormat(metadata.value(), metadata_definition.format()));
    ASSIGN_OR_RETURN(
        auto value,
        IrValueToNormalizedByteString(
            metadata.value(), metadata_definition.metadata().bitwidth()));
    pi_metadata.set_value(NormalizedToCanonicalByteString(value));
    *result.add_metadata() = pi_metadata;
  }
  // Check for missing metadata
  for (const auto &item : info.packet_in_metadata_by_name()) {
    const auto &name = item.first;
    const auto &meta = item.second;
    if (!used_metadata_names.contains(name)) {
      return gutil::InvalidArgumentErrorBuilder()
             << "\"" << kind << "\" metadata \"" << meta.metadata().name()
             << "\" with id " << meta.metadata().id()
             << " is missing in P4Info.";
    }
  }

  return result;
}

}  // namespace

gutil::StatusOr<IrTableEntry> PiTableEntryToIr(const IrP4Info &info,
                                               const p4::v1::TableEntry &pi) {
  IrTableEntry ir;
  ASSIGN_OR_RETURN(
      const auto &table,
      gutil::FindOrStatus(info.tables_by_id(), pi.table_id()),
      _ << "Table ID " << pi.table_id() << " does not exist in P4Info.");
  ir.set_table_name(table.preamble().alias());

  // Validate and translate the matches
  absl::flat_hash_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  for (const auto pi_match : pi.match()) {
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_field_ids, pi_match.field_id(),
        absl::StrCat("Duplicate match field found with ID ",
                     pi_match.field_id(), ".")));

    ASSIGN_OR_RETURN(
        const auto &match,
        gutil::FindOrStatus(table.match_fields_by_id(), pi_match.field_id()),
        _ << "Match Field " << pi_match.field_id()
          << " does not exist in table \"" << ir.table_name() << "\".");
    ASSIGN_OR_RETURN(const auto &match_entry,
                     PiMatchFieldToIr(info, match, pi_match));
    *ir.add_matches() = match_entry;

    if (match.match_field().match_type() == MatchField::EXACT) {
      ++mandatory_matches;
    }
  }

  int expected_mandatory_matches = GetNumMandatoryMatches(table);
  if (mandatory_matches != expected_mandatory_matches) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected " << expected_mandatory_matches
           << " mandatory match conditions but found " << mandatory_matches
           << " instead.";
  }

  // Validate and translate the action.
  if (!pi.has_action()) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Action missing in TableEntry with ID " << pi.table_id() << ".";
  }
  ASSIGN_OR_RETURN(const auto &action_entry,
                   PiActionInvocationToIr(info, pi.action(), table.actions()));
  *ir.mutable_action() = action_entry;

  return ir;
}

gutil::StatusOr<p4::v1::TableEntry> IrTableEntryToPi(const IrP4Info &info,
                                                     const IrTableEntry &ir) {
  p4::v1::TableEntry pi;
  ASSIGN_OR_RETURN(const auto &table,
                   gutil::FindOrStatus(info.tables_by_name(), ir.table_name()),
                   _ << "Table name \"" << ir.table_name()
                     << "\" does not exist in P4Info.");
  pi.set_table_id(table.preamble().id());

  // Validate and translate the matches
  absl::flat_hash_set<std::string> used_field_names;
  int mandatory_matches = 0;
  for (const auto ir_match : ir.matches()) {
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_field_names, ir_match.name(),
        absl::StrCat("Duplicate match field found with name \"",
                     ir_match.name(), "\".")));

    ASSIGN_OR_RETURN(
        const auto &match,
        gutil::FindOrStatus(table.match_fields_by_name(), ir_match.name()),
        _ << "Match Field \"" << ir_match.name()
          << "\" does not exist in table \"" << ir.table_name() << "\".");
    ASSIGN_OR_RETURN(const auto &match_entry,
                     IrMatchFieldToPi(info, match, ir_match));
    *pi.add_match() = match_entry;

    if (match.match_field().match_type() == MatchField::EXACT) {
      ++mandatory_matches;
    }
  }

  int expected_mandatory_matches = GetNumMandatoryMatches(table);
  if (mandatory_matches != expected_mandatory_matches) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected " << expected_mandatory_matches
           << " mandatory match conditions but found " << mandatory_matches
           << " instead.";
  }

  // Validate and translate the action.
  if (!ir.has_action()) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Action missing in TableEntry with name \"" << ir.table_name()
           << "\".";
  }
  ASSIGN_OR_RETURN(const auto &action_entry,
                   IrActionInvocationToPi(info, ir.action(), table.actions()));
  *pi.mutable_action() = action_entry;

  return pi;
}

gutil::StatusOr<IrPacketIn> PiPacketInToIr(const IrP4Info &info,
                                           const p4::v1::PacketIn &packet) {
  return PiPacketIoToIr<p4::v1::PacketIn, IrPacketIn>(info, "packet-in",
                                                      packet);
}
gutil::StatusOr<IrPacketOut> PiPacketOutToIr(const IrP4Info &info,
                                             const p4::v1::PacketOut &packet) {
  return PiPacketIoToIr<p4::v1::PacketOut, IrPacketOut>(info, "packet-out",
                                                        packet);
}

gutil::StatusOr<p4::v1::PacketIn> IrPacketInToPi(const IrP4Info &info,
                                                 const IrPacketIn &packet) {
  return IrPacketIoToPi<p4::v1::PacketIn, IrPacketIn>(info, "packet-in",
                                                      packet);
}
gutil::StatusOr<p4::v1::PacketOut> IrPacketOutToPi(const IrP4Info &info,
                                                   const IrPacketOut &packet) {
  return IrPacketIoToPi<p4::v1::PacketOut, IrPacketOut>(info, "packet-out",
                                                        packet);
}

}  // namespace pdpi
