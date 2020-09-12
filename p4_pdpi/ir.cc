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

#include <ctype.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/map.h"
#include "google/protobuf/repeated_field.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "gutil/collections.h"
#include "gutil/status.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/config/v1/p4types.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/ir.h"

namespace pdpi {

using ::absl::StatusOr;
using ::gutil::InvalidArgumentErrorBuilder;
using ::gutil::UnimplementedErrorBuilder;
using ::p4::config::v1::MatchField;
using ::p4::config::v1::P4TypeInfo;
using ::pdpi::Format;
using ::pdpi::IrActionDefinition;
using ::pdpi::IrActionInvocation;
using ::pdpi::IrMatchFieldDefinition;
using ::pdpi::IrP4Info;
using ::pdpi::IrTableDefinition;

namespace {
// Helper for GetFormat that extracts the necessary info from a P4Info
// element. T could be p4::config::v1::ControllerPacketMetadata::Metadata,
// p4::config::v1::MatchField, or p4::config::v1::Action::Param (basically
// anything that has a set of annotations, a bitwidth and named type
// information).
template <typename T>
StatusOr<Format> GetFormatForP4InfoElement(const T &element,
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
    return InvalidArgumentErrorBuilder()
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
StatusOr<uint32_t> GetNumberInAnnotation(
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
      return absl::OkStatus();
    case p4::config::v1::MatchField::EXACT:
    case p4::config::v1::MatchField::OPTIONAL:
      return absl::OkStatus();
    default:
      return InvalidArgumentErrorBuilder()
             << "Match field match type not supported: "
             << match.match_field().ShortDebugString() << ".";
  }
}

}  // namespace

StatusOr<IrP4Info> CreateIrP4Info(const p4::config::v1::P4Info &p4_info) {
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
    for (const auto &match_field : table.match_fields()) {
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
      return UnimplementedErrorBuilder()
             << "A WCMP table must have a @oneshot annotation, but \""
             << table.preamble().alias()
             << "\" is not valid. is_wcmp = " << is_wcmp
             << ", has_oneshot = " << has_oneshot << ".";
    }
    if (is_wcmp) {
      ir_table_definition.set_uses_oneshot(true);
      ASSIGN_OR_RETURN(
          const uint32_t weight_proto_id,
          GetNumberInAnnotation(table.preamble().annotations(),
                                "weight_proto_id"),
          _ << "WCMP table \"" << table.preamble().alias()
            << "\" does not have a valid @weight_proto_id annotation.");
      ir_table_definition.set_weight_proto_id(weight_proto_id);
    }

    p4::config::v1::ActionRef default_action_ref;
    for (const auto &action_ref : table.action_refs()) {
      IrActionReference ir_action_reference;
      *ir_action_reference.mutable_ref() = action_ref;
      // Make sure the action is defined
      ASSIGN_OR_RETURN(
          *ir_action_reference.mutable_action(),
          gutil::FindOrStatus(info.actions_by_id(), action_ref.id()),
          _ << "Missing definition for action with id " << action_ref.id()
            << ".");
      if (action_ref.scope() == p4::config::v1::ActionRef::DEFAULT_ONLY) {
        *ir_table_definition.add_default_only_actions() = ir_action_reference;
      } else {
        uint32_t proto_id = 0;
        ASSIGN_OR_RETURN(
            proto_id,
            GetNumberInAnnotation(action_ref.annotations(), "proto_id"),
            _ << "Action \"" << ir_action_reference.action().preamble().name()
              << "\" in table \"" << table.preamble().alias()
              << "\" does not have a valid @proto_id annotation.");
        ir_action_reference.set_proto_id(proto_id);
        *ir_table_definition.add_entry_actions() = ir_action_reference;
      }
    }
    if (table.const_default_action_id() != 0) {
      const uint32_t const_default_action_id = table.const_default_action_id();
      IrActionReference const_default_action_reference;

      // The const_default_action should always point to a table action.
      for (const auto &action : ir_table_definition.default_only_actions()) {
        if (action.ref().id() == const_default_action_id) {
          const_default_action_reference = action;
          break;
        }
      }
      if (const_default_action_reference.ref().id() == 0) {
        for (const auto &action : ir_table_definition.entry_actions()) {
          if (action.ref().id() == const_default_action_id) {
            const_default_action_reference = action;
            break;
          }
        }
      }
      if (const_default_action_reference.ref().id() == 0) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Table \"" << table.preamble().alias()
               << "\" default action id " << table.const_default_action_id()
               << " does not match any of the table's actions.";
      }

      *ir_table_definition.mutable_const_default_action() =
          const_default_action_reference.action();
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
      return InvalidArgumentErrorBuilder()
             << "Unknown controller packet metadata: " << kind
             << ". Only packet_in and packet_out are supported.";
    }
  }

  // Counters.
  for (const auto &counter : p4_info.direct_counters()) {
    const auto table_id = counter.direct_table_id();
    RETURN_IF_ERROR(gutil::FindOrStatus(info.tables_by_id(), table_id).status())
        << "Missing table " << table_id << " for counter with ID "
        << counter.preamble().id() << ".";
    IrCounter ir_counter;
    ir_counter.set_unit(counter.spec().unit());

    // Add to tables_by_id and tables_by_name.
    auto &table1 = (*info.mutable_tables_by_id())[table_id];
    auto &table2 = (*info.mutable_tables_by_name())[table1.preamble().alias()];
    *table1.mutable_counter() = ir_counter;
    *table2.mutable_counter() = ir_counter;
  }

  // Meters.
  for (const auto &meter : p4_info.direct_meters()) {
    const auto table_id = meter.direct_table_id();
    RETURN_IF_ERROR(gutil::FindOrStatus(info.tables_by_id(), table_id).status())
        << "Missing table " << table_id << " for meter with ID "
        << meter.preamble().id() << ".";
    IrMeter ir_meter;
    ir_meter.set_unit(meter.spec().unit());

    // Add to tables_by_id and tables_by_name.
    auto &table1 = (*info.mutable_tables_by_id())[table_id];
    auto &table2 = (*info.mutable_tables_by_name())[table1.preamble().alias()];
    *table1.mutable_meter() = ir_meter;
    *table2.mutable_meter() = ir_meter;
  }

  return info;
}

namespace {

// Verifies the contents of the PI representation and translates to the IR
// message
StatusOr<IrMatch> PiMatchFieldToIr(
    const IrP4Info &info, const IrMatchFieldDefinition &ir_match_definition,
    const p4::v1::FieldMatch &pi_match) {
  IrMatch match_entry;
  const MatchField &match_field = ir_match_definition.match_field();
  uint32_t bitwidth = match_field.bitwidth();

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!pi_match.has_exact()) {
        return InvalidArgumentErrorBuilder()
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
        return InvalidArgumentErrorBuilder()
               << "Expected LPM match type in PI.";
      }

      uint32_t prefix_len = pi_match.lpm().prefix_len();
      if (prefix_len > bitwidth) {
        return InvalidArgumentErrorBuilder()
               << "Prefix length " << prefix_len << " is greater than bitwidth "
               << bitwidth << " in LPM.";
      }

      if (prefix_len == 0) {
        return InvalidArgumentErrorBuilder()
               << "A wild-card LPM match (i.e., prefix length of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_name(match_field.name());
      ASSIGN_OR_RETURN(const auto mask, PrefixLenToMask(prefix_len, bitwidth));
      ASSIGN_OR_RETURN(const auto value, ArbitraryToNormalizedByteString(
                                             pi_match.lpm().value(), bitwidth));
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return InvalidArgumentErrorBuilder()
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
        return InvalidArgumentErrorBuilder()
               << "Expected ternary match type in PI.";
      }

      ASSIGN_OR_RETURN(const auto &value,
                       ArbitraryToNormalizedByteString(
                           pi_match.ternary().value(), bitwidth));
      ASSIGN_OR_RETURN(
          const auto &mask,
          ArbitraryToNormalizedByteString(pi_match.ternary().mask(), bitwidth));

      if (IsAllZeros(mask)) {
        return InvalidArgumentErrorBuilder()
               << "A wild-card ternary match (i.e., mask of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_name(match_field.name());
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return InvalidArgumentErrorBuilder()
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
    case MatchField::OPTIONAL: {
      if (!pi_match.has_optional()) {
        return InvalidArgumentErrorBuilder()
               << "Expected optional match type in PI.";
      }

      match_entry.set_name(match_field.name());
      ASSIGN_OR_RETURN(
          *match_entry.mutable_optional()->mutable_value(),
          ArbitraryByteStringToIrValue(ir_match_definition.format(), bitwidth,
                                       pi_match.optional().value()));
      break;
    }
    default:
      return InvalidArgumentErrorBuilder()
             << "Unsupported match type \""
             << MatchField_MatchType_Name(match_field.match_type())
             << "\" in \"" << match_entry.name() << "\".";
  }
  return match_entry;
}

// Verifies the contents of the IR representation and translates to the PI
// message
StatusOr<p4::v1::FieldMatch> IrMatchFieldToPi(
    const IrP4Info &info, const IrMatchFieldDefinition &ir_match_definition,
    const IrMatch &ir_match) {
  p4::v1::FieldMatch match_entry;
  const MatchField &match_field = ir_match_definition.match_field();
  uint32_t bitwidth = match_field.bitwidth();

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!ir_match.has_exact()) {
        return InvalidArgumentErrorBuilder()
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
        return InvalidArgumentErrorBuilder()
               << "Expected LPM match type in IR table entry.";
      }

      uint32_t prefix_len = ir_match.lpm().prefix_length();
      if (prefix_len > bitwidth) {
        return InvalidArgumentErrorBuilder()
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
        return InvalidArgumentErrorBuilder()
               << "A wild-card LPM match (i.e., prefix length of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_field_id(match_field.id());
      ASSIGN_OR_RETURN(const auto mask, PrefixLenToMask(prefix_len, bitwidth));
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return InvalidArgumentErrorBuilder()
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
        return InvalidArgumentErrorBuilder()
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
        return InvalidArgumentErrorBuilder()
               << "A wild-card ternary match (i.e., mask of 0) must be "
                  "represented by omitting the match altogether.";
      }
      match_entry.set_field_id(match_field.id());
      ASSIGN_OR_RETURN(const auto intersection, Intersection(value, mask));
      if (value != intersection) {
        return InvalidArgumentErrorBuilder()
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
    case MatchField::OPTIONAL: {
      if (!ir_match.has_optional()) {
        return InvalidArgumentErrorBuilder()
               << "Expected optional match type in IR table entry.";
      }

      match_entry.set_field_id(match_field.id());
      RETURN_IF_ERROR(ValidateIrValueFormat(ir_match.optional().value(),
                                            ir_match_definition.format()));
      ASSIGN_OR_RETURN(const auto &value,
                       IrValueToNormalizedByteString(
                           ir_match.optional().value(),
                           ir_match_definition.match_field().bitwidth()));
      match_entry.mutable_optional()->set_value(
          NormalizedToCanonicalByteString(value));
      break;
    }
    default:
      return InvalidArgumentErrorBuilder()
             << "Unsupported match type \""
             << MatchField_MatchType_Name(match_field.match_type()) << "\" in "
             << "match field with id " << match_entry.field_id() << ".";
  }
  return match_entry;
}

// Translates the action invocation from its PI form to IR.
StatusOr<IrActionInvocation> PiActionToIr(
    const IrP4Info &info, const p4::v1::Action &pi_action,
    const google::protobuf::RepeatedPtrField<IrActionReference>
        &valid_actions) {
  IrActionInvocation action_entry;
  uint32_t action_id = pi_action.action_id();

  ASSIGN_OR_RETURN(
      const auto &ir_action_definition,
      gutil::FindOrStatus(info.actions_by_id(), action_id),
      _ << "Action ID " << action_id << " does not exist in P4Info.");

  if (absl::c_find_if(valid_actions,
                      [action_id](const IrActionReference &action) {
                        return action.action().preamble().id() == action_id;
                      }) == valid_actions.end()) {
    return InvalidArgumentErrorBuilder()
           << "Action ID " << action_id
           << " is not a valid action for this table.";
  }

  int action_params_size = ir_action_definition.params_by_id().size();
  if (action_params_size != pi_action.params().size()) {
    return InvalidArgumentErrorBuilder()
           << "Expected " << action_params_size << " parameters, but got "
           << pi_action.params().size() << " instead in action with ID "
           << action_id << ".";
  }
  action_entry.set_name(ir_action_definition.preamble().alias());
  absl::flat_hash_set<uint32_t> used_params;
  for (const auto &param : pi_action.params()) {
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_params, param.param_id(),
        absl::StrCat("Duplicate param field found with ID ", param.param_id(),
                     ".")));

    ASSIGN_OR_RETURN(const auto &ir_param_definition,
                     gutil::FindOrStatus(ir_action_definition.params_by_id(),
                                         param.param_id()),
                     _ << "Unable to find param ID " << param.param_id()
                       << " in action with ID " << action_id);
    IrActionInvocation::IrActionParam *param_entry = action_entry.add_params();
    param_entry->set_name(ir_param_definition.param().name());
    ASSIGN_OR_RETURN(
        *param_entry->mutable_value(),
        ArbitraryByteStringToIrValue(ir_param_definition.format(),
                                     ir_param_definition.param().bitwidth(),
                                     param.value()));
  }
  return action_entry;
}

// Translates the action invocation from its IR form to PI.
StatusOr<p4::v1::Action> IrActionInvocationToPi(
    const IrP4Info &info, const IrActionInvocation &ir_table_action,
    const google::protobuf::RepeatedPtrField<IrActionReference>
        &valid_actions) {
  const std::string &action_name = ir_table_action.name();

  ASSIGN_OR_RETURN(
      const auto &ir_action_definition,
      gutil::FindOrStatus(info.actions_by_name(), action_name),
      _ << "Action \"" << action_name << "\" does not exist in P4Info.");

  if (absl::c_find_if(
          valid_actions, [action_name](const IrActionReference &action) {
            return action.action().preamble().alias() == action_name;
          }) == valid_actions.end()) {
    return InvalidArgumentErrorBuilder()
           << "Action \"" << action_name
           << "\" is not a valid action for this table.";
  }

  int action_params_size = ir_action_definition.params_by_name().size();
  if (action_params_size != ir_table_action.params().size()) {
    return InvalidArgumentErrorBuilder()
           << "Expected " << action_params_size << " parameters, but got "
           << ir_table_action.params().size() << " instead in action \""
           << action_name << "\".";
  }

  p4::v1::Action action;
  action.set_action_id(ir_action_definition.preamble().id());
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
    p4::v1::Action_Param *param_entry = action.add_params();
    param_entry->set_param_id(ir_param_definition.param().id());
    RETURN_IF_ERROR(
        ValidateIrValueFormat(param.value(), ir_param_definition.format()));
    ASSIGN_OR_RETURN(
        const auto &value,
        IrValueToNormalizedByteString(param.value(),
                                      ir_param_definition.param().bitwidth()));
    param_entry->set_value(NormalizedToCanonicalByteString(value));
  }
  return action;
}

// Translates the action set from its PI form to IR.
StatusOr<IrActionSet> PiActionSetToIr(
    const IrP4Info &info, const p4::v1::ActionProfileActionSet &pi_action_set,
    const google::protobuf::RepeatedPtrField<IrActionReference>
        &valid_actions) {
  IrActionSet ir_action_set;
  for (const auto &pi_profile_action : pi_action_set.action_profile_actions()) {
    auto *ir_action = ir_action_set.add_actions();
    ASSIGN_OR_RETURN(
        *ir_action->mutable_action(),
        PiActionToIr(info, pi_profile_action.action(), valid_actions));

    // A action set weight that is not positive does not make sense on a switch.
    if (pi_profile_action.weight() < 1) {
      return InvalidArgumentErrorBuilder()
             << "Expected positive action set weight, but got "
             << pi_profile_action.weight() << " instead.";
    }
    ir_action->set_weight(pi_profile_action.weight());
  }
  return ir_action_set;
}

// Translates the action set from its IR form to PI.
StatusOr<p4::v1::ActionProfileActionSet> IrActionSetToPi(
    const IrP4Info &info, const IrActionSet &ir_action_set,
    const google::protobuf::RepeatedPtrField<IrActionReference>
        &valid_actions) {
  p4::v1::ActionProfileActionSet pi;
  for (const auto &ir_action : ir_action_set.actions()) {
    auto *pi_action = pi.add_action_profile_actions();
    ASSIGN_OR_RETURN(
        *pi_action->mutable_action(),
        IrActionInvocationToPi(info, ir_action.action(), valid_actions));
    if (ir_action.weight() < 1) {
      return InvalidArgumentErrorBuilder()
             << "Expected positive action set weight, but got "
             << ir_action.weight() << " instead.";
    }
    pi_action->set_weight(ir_action.weight());
  }
  return pi;
}

// Generic helper that works for both packet-in and packet-out. For both, I is
// one of p4::v1::{PacketIn, PacketOut} and O is one of {IrPacketIn,
// IrPacketOut}.
template <typename I, typename O>
StatusOr<O> PiPacketIoToIr(const IrP4Info &info, const std::string &kind,
                           const I &packet) {
  O result;
  result.set_payload(packet.payload());
  absl::flat_hash_set<uint32_t> used_metadata_ids;

  google::protobuf::Map<uint32_t, IrPacketIoMetadataDefinition> metadata_by_id;
  if (kind == "packet-in") {
    metadata_by_id = info.packet_in_metadata_by_id();
  } else if (kind == "packet-out") {
    metadata_by_id = info.packet_out_metadata_by_id();
  } else {
    return InvalidArgumentErrorBuilder() << "Invalid PacketIo type " << kind;
  }

  for (const auto &metadata : packet.metadata()) {
    uint32_t id = metadata.metadata_id();
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_metadata_ids, id,
        absl::StrCat("Duplicate \"", kind, "\" metadata found with ID ", id,
                     ".")));

    ASSIGN_OR_RETURN(
        const auto &metadata_definition,
        gutil::FindOrStatus(metadata_by_id, id),
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
  for (const auto &item : metadata_by_id) {
    const auto &id = item.first;
    const auto &meta = item.second;
    if (!used_metadata_ids.contains(id)) {
      return InvalidArgumentErrorBuilder()
             << "\"" << kind << "\" metadata \"" << meta.metadata().name()
             << "\" with ID " << id << " is missing.";
    }
  }

  return result;
}

template <typename I, typename O>
StatusOr<I> IrPacketIoToPi(const IrP4Info &info, const std::string &kind,
                           const O &packet) {
  I result;
  result.set_payload(packet.payload());
  absl::flat_hash_set<std::string> used_metadata_names;
  google::protobuf::Map<std::string, IrPacketIoMetadataDefinition>
      metadata_by_name;
  if (kind == "packet-in") {
    metadata_by_name = info.packet_in_metadata_by_name();
  } else if (kind == "packet-out") {
    metadata_by_name = info.packet_out_metadata_by_name();
  } else {
    return InvalidArgumentErrorBuilder() << "Invalid PacketIo type " << kind;
  }

  for (const auto &metadata : packet.metadata()) {
    const std::string &name = metadata.name();
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        used_metadata_names, name,
        absl::StrCat("Duplicate \"", kind, "\" metadata found with name \"",
                     name, "\".")));

    ASSIGN_OR_RETURN(const auto &metadata_definition,
                     gutil::FindOrStatus(metadata_by_name, name),
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
  for (const auto &item : metadata_by_name) {
    const auto &name = item.first;
    const auto &meta = item.second;
    if (!used_metadata_names.contains(name)) {
      return InvalidArgumentErrorBuilder()
             << "\"" << kind << "\" metadata \"" << meta.metadata().name()
             << "\" with id " << meta.metadata().id() << " is missing.";
    }
  }

  return result;
}

}  // namespace

StatusOr<IrTableEntry> PiTableEntryToIr(const IrP4Info &info,
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
  for (const auto &pi_match : pi.match()) {
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
    return InvalidArgumentErrorBuilder()
           << "Expected " << expected_mandatory_matches
           << " mandatory match conditions but found " << mandatory_matches
           << " instead.";
  }

  if (RequiresPriority(table)) {
    if (pi.priority() <= 0) {
      return InvalidArgumentErrorBuilder()
             << "Table entries with ternary or optional matches require a "
                "positive non-zero "
                "priority. Got "
             << pi.priority() << " instead.";
    } else {
      ir.set_priority(pi.priority());
    }
  } else if (pi.priority() != 0) {
    return InvalidArgumentErrorBuilder() << "Table entries with no ternary or "
                                            "optional matches cannot have a "
                                            "priority. Got "
                                         << pi.priority() << " instead.";
  }

  // Validate and translate the action.
  if (!pi.has_action()) {
    return InvalidArgumentErrorBuilder()
           << "Action missing in TableEntry with ID " << pi.table_id() << ".";
  }
  switch (pi.action().type_case()) {
    case p4::v1::TableAction::kAction: {
      if (table.uses_oneshot()) {
        return InvalidArgumentErrorBuilder()
               << "Table \"" << ir.table_name()
               << "\" requires an action set since it uses onseshot. Got "
                  "action instead.";
      }
      ASSIGN_OR_RETURN(
          *ir.mutable_action(),
          PiActionToIr(info, pi.action().action(), table.entry_actions()));
      break;
    }
    case p4::v1::TableAction::kActionProfileActionSet: {
      if (!table.uses_oneshot()) {
        return InvalidArgumentErrorBuilder()
               << "Table \"" << ir.table_name()
               << "\" requires an action since it does not use onseshot. Got "
                  "action set instead.";
      }
      ASSIGN_OR_RETURN(
          *ir.mutable_action_set(),
          PiActionSetToIr(info, pi.action().action_profile_action_set(),
                          table.entry_actions()));
      break;
    }
    default: {
      return gutil::UnimplementedErrorBuilder()
             << "Unsupported action type: " << pi.action().type_case();
    }
  }

  return ir;
}

StatusOr<p4::v1::TableEntry> IrTableEntryToPi(const IrP4Info &info,
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
  for (const auto &ir_match : ir.matches()) {
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
    return InvalidArgumentErrorBuilder()
           << "Expected " << expected_mandatory_matches
           << " mandatory match conditions but found " << mandatory_matches
           << " instead.";
  }

  if (RequiresPriority(table)) {
    if (ir.priority() <= 0) {
      return InvalidArgumentErrorBuilder()
             << "Table entries with ternary or optional matches require a "
                "positive non-zero "
                "priority. Got "
             << ir.priority() << " instead.";
    } else {
      pi.set_priority(ir.priority());
    }
  } else if (ir.priority() != 0) {
    return InvalidArgumentErrorBuilder() << "Table entries with no ternary or "
                                            "optional matches require a zero "
                                            "priority. Got "
                                         << ir.priority() << " instead.";
  }

  // Validate and translate the action.
  switch (ir.type_case()) {
    case IrTableEntry::kAction: {
      if (table.uses_oneshot()) {
        return InvalidArgumentErrorBuilder()
               << "Table \"" << ir.table_name()
               << "\" requires an action set since it uses onseshot. Got "
                  "action instead.";
      }
      ASSIGN_OR_RETURN(
          *pi.mutable_action()->mutable_action(),
          IrActionInvocationToPi(info, ir.action(), table.entry_actions()));
      break;
    }
    case IrTableEntry::kActionSet: {
      if (!table.uses_oneshot()) {
        return InvalidArgumentErrorBuilder()
               << "Table \"" << ir.table_name()
               << "\" requires an action since it does not use onseshot. Got "
                  "action set instead.";
      }
      ASSIGN_OR_RETURN(
          *pi.mutable_action()->mutable_action_profile_action_set(),
          IrActionSetToPi(info, ir.action_set(), table.entry_actions()));
      break;
    }
    default: {
      return InvalidArgumentErrorBuilder()
             << "Action missing in TableEntry with name \"" << ir.table_name()
             << "\".";
    }
  }
  return pi;
}

StatusOr<IrPacketIn> PiPacketInToIr(const IrP4Info &info,
                                    const p4::v1::PacketIn &packet) {
  return PiPacketIoToIr<p4::v1::PacketIn, IrPacketIn>(info, "packet-in",
                                                      packet);
}
StatusOr<IrPacketOut> PiPacketOutToIr(const IrP4Info &info,
                                      const p4::v1::PacketOut &packet) {
  return PiPacketIoToIr<p4::v1::PacketOut, IrPacketOut>(info, "packet-out",
                                                        packet);
}

StatusOr<p4::v1::PacketIn> IrPacketInToPi(const IrP4Info &info,
                                          const IrPacketIn &packet) {
  return IrPacketIoToPi<p4::v1::PacketIn, IrPacketIn>(info, "packet-in",
                                                      packet);
}
StatusOr<p4::v1::PacketOut> IrPacketOutToPi(const IrP4Info &info,
                                            const IrPacketOut &packet) {
  return IrPacketIoToPi<p4::v1::PacketOut, IrPacketOut>(info, "packet-out",
                                                        packet);
}

StatusOr<IrReadRequest> PiReadRequestToIr(
    const IrP4Info &info, const p4::v1::ReadRequest &read_request) {
  IrReadRequest result;
  if (read_request.device_id() == 0) {
    return InvalidArgumentErrorBuilder() << "Device ID missing.";
  }
  result.set_device_id(read_request.device_id());
  std::string base = "Only wildcard reads of all table entries are supported. ";
  if (read_request.entities().size() != 1) {
    return UnimplementedErrorBuilder()
           << base << "Only 1 entity is supported. Found "
           << read_request.entities().size() << " entities in read request.";
  }
  if (!read_request.entities(0).has_table_entry()) {
    return UnimplementedErrorBuilder()
           << base << "Found an entity that is not a table entry.";
  }
  const p4::v1::TableEntry entry = read_request.entities(0).table_entry();
  if (entry.table_id() != 0 || entry.priority() != 0 ||
      entry.controller_metadata() != 0 || entry.idle_timeout_ns() != 0 ||
      entry.is_default_action() || !entry.metadata().empty() ||
      entry.has_action() || entry.has_time_since_last_hit() ||
      !entry.match().empty()) {
    return UnimplementedErrorBuilder()
           << base
           << "At least one field (other than counter_data and meter_config is "
              "set in the table entry.";
  }
  if (entry.has_meter_config()) {
    if (entry.meter_config().ByteSizeLong() != 0) {
      return UnimplementedErrorBuilder()
             << base << "Found a non-empty meter_config in table entry.";
    }
    result.set_read_meter_configs(true);
  }
  if (entry.has_counter_data()) {
    if (entry.counter_data().ByteSizeLong() != 0) {
      return UnimplementedErrorBuilder()
             << base << "Found a non-empty counter_data in table entry.";
    }
    result.set_read_counter_data(true);
  }
  return result;
}

StatusOr<p4::v1::ReadRequest> IrReadRequestToPi(
    const IrP4Info &info, const IrReadRequest &read_request) {
  p4::v1::ReadRequest result;
  if (read_request.device_id() == 0) {
    return UnimplementedErrorBuilder() << "Device ID missing.";
  }
  result.set_device_id(read_request.device_id());
  p4::v1::TableEntry *entry = result.add_entities()->mutable_table_entry();
  if (read_request.read_counter_data()) {
    entry->mutable_counter_data();
  }
  if (read_request.read_meter_configs()) {
    entry->mutable_meter_config();
  }
  return result;
}

StatusOr<IrReadResponse> PiReadResponseToIr(
    const IrP4Info &info, const p4::v1::ReadResponse &read_response) {
  IrReadResponse result;
  for (const auto &entity : read_response.entities()) {
    if (!entity.has_table_entry()) {
      return UnimplementedErrorBuilder()
             << "Only table entries are supported in ReadResponse.";
    }
    ASSIGN_OR_RETURN(*result.add_table_entries(),
                     PiTableEntryToIr(info, entity.table_entry()));
  }
  return result;
}

StatusOr<p4::v1::ReadResponse> IrReadResponseToPi(
    const IrP4Info &info, const IrReadResponse &read_response) {
  p4::v1::ReadResponse result;
  for (const auto &entity : read_response.table_entries()) {
    ASSIGN_OR_RETURN(*result.add_entities()->mutable_table_entry(),
                     IrTableEntryToPi(info, entity));
  }
  return result;
}

StatusOr<IrUpdate> PiUpdateToIr(const IrP4Info &info,
                                const p4::v1::Update &update) {
  IrUpdate ir_update;
  if (!update.entity().has_table_entry()) {
    return UnimplementedErrorBuilder()
           << "Only table entries are supported in Update.";
  }
  if (update.type() == p4::v1::Update_Type_UNSPECIFIED) {
    return InvalidArgumentErrorBuilder() << "Update type should be specified.";
  }
  ir_update.set_type(update.type());
  ASSIGN_OR_RETURN(*ir_update.mutable_table_entry(),
                   PiTableEntryToIr(info, update.entity().table_entry()));
  return ir_update;
}

StatusOr<p4::v1::Update> IrUpdateToPi(const IrP4Info &info,
                                      const IrUpdate &update) {
  p4::v1::Update pi_update;

  if (!p4::v1::Update_Type_IsValid(update.type())) {
    return InvalidArgumentErrorBuilder()
           << "Invalid type value: " << update.type();
  }
  if (update.type() == p4::v1::Update_Type_UNSPECIFIED) {
    return InvalidArgumentErrorBuilder() << "Update type should be specified.";
  }
  pi_update.set_type(update.type());
  ASSIGN_OR_RETURN(*pi_update.mutable_entity()->mutable_table_entry(),
                   IrTableEntryToPi(info, update.table_entry()));
  return pi_update;
}

StatusOr<IrWriteRequest> PiWriteRequestToIr(
    const IrP4Info &info, const p4::v1::WriteRequest &write_request) {
  IrWriteRequest ir_write_request;

  if (write_request.role_id() != 0) {
    return InvalidArgumentErrorBuilder()
           << "Only the default role is supported, but got role ID "
           << write_request.role_id() << "instead.";
  }

  if (write_request.atomicity() !=
      p4::v1::WriteRequest_Atomicity_CONTINUE_ON_ERROR) {
    return InvalidArgumentErrorBuilder()
           << "Only CONTINUE_ON_ERROR is supported for atomicity.";
  }

  ir_write_request.set_device_id(write_request.device_id());
  if (write_request.election_id().high() > 0 ||
      write_request.election_id().low() > 0) {
    *ir_write_request.mutable_election_id() = write_request.election_id();
  }

  for (const auto &update : write_request.updates()) {
    ASSIGN_OR_RETURN(*ir_write_request.add_updates(),
                     PiUpdateToIr(info, update));
  }
  return ir_write_request;
}

StatusOr<p4::v1::WriteRequest> IrWriteRequestToPi(
    const IrP4Info &info, const IrWriteRequest &ir_write_request) {
  p4::v1::WriteRequest pi_write_request;

  pi_write_request.set_role_id(0);
  pi_write_request.set_atomicity(
      p4::v1::WriteRequest_Atomicity_CONTINUE_ON_ERROR);
  pi_write_request.set_device_id(ir_write_request.device_id());
  if (ir_write_request.election_id().high() > 0 ||
      ir_write_request.election_id().low() > 0) {
    *pi_write_request.mutable_election_id() = ir_write_request.election_id();
  }

  for (const auto &update : ir_write_request.updates()) {
    ASSIGN_OR_RETURN(*pi_write_request.add_updates(),
                     IrUpdateToPi(info, update));
  }
  return pi_write_request;
}
// Formats a grpc status about write request into a readible string.
std::string WriteRequestGrpcStatusToString(const grpc::Status &status) {
  std::string readable_status = absl::StrCat(
      "gRPC_error_code: ", status.error_code(), "\n",
      "gRPC_error_message: ", "\"", status.error_message(), "\"", "\n");
  if (status.error_details().empty()) {
    absl::StrAppend(&readable_status, "gRPC_error_details: <empty>\n");
  } else {
    google::rpc::Status inner_status;
    if (inner_status.ParseFromString(status.error_details())) {
      absl::StrAppend(&readable_status, "details in google.rpc.Status:\n",
                      "inner_status.code:", inner_status.code(),
                      "\n"
                      "inner_status.message:\"",
                      inner_status.message(), "\"\n",
                      "inner_status.details:\n");
      p4::v1::Error p4_error;
      for (const auto &inner_status_detail : inner_status.details()) {
        absl::StrAppend(&readable_status, "  ");
        if (inner_status_detail.UnpackTo(&p4_error)) {
          absl::StrAppend(
              &readable_status, "error_status: ",
              absl::StatusCodeToString(
                  static_cast<absl::StatusCode>(p4_error.canonical_code())));
          absl::StrAppend(&readable_status, " error_message: ", "\"",
                          p4_error.message(), "\"", "\n");
        } else {
          absl::StrAppend(&readable_status, "<Can not unpack p4error>\n");
        }
      }
    } else {
      absl::StrAppend(&readable_status,
                      "<Can not parse google::rpc::status>\n");
    }
  }
  return readable_status;
}

absl::StatusOr<IrWriteRpcStatus> GrpcStatusToIrWriteRpcStatus(
    const grpc::Status &grpc_status, int number_of_updates_in_write_request) {
  IrWriteRpcStatus ir_write_status;
  if (grpc_status.ok()) {
    // If all batch updates succeeded, `status` is ok and neither error_message
    // nor error_details is populated. If either error_message or error_details
    // is populated, `status` is ill-formed and should return
    // InvalidArgumentError.
    if (!grpc_status.error_message().empty() ||
        !grpc_status.error_details().empty()) {
      return gutil::InvalidArgumentErrorBuilder()
             << "gRPC status can not be ok and contain an error message or "
                "error details.";
    }
    ir_write_status.mutable_rpc_response();
    for (int i = 0; i < number_of_updates_in_write_request; i++) {
      ir_write_status.mutable_rpc_response()->add_statuses()->set_code(
          ::google::rpc::OK);
    }
    return ir_write_status;
  } else if (!grpc_status.ok() && grpc_status.error_details().empty()) {
    // Rpc-wide error
    RETURN_IF_ERROR(
        IsGoogleRpcCode(static_cast<int>(grpc_status.error_code())));
    RETURN_IF_ERROR(ValidateGenericUpdateStatus(
        static_cast<google::rpc::Code>(grpc_status.error_code()),
        grpc_status.error_message()));
    ir_write_status.mutable_rpc_wide_error()->set_code(
        static_cast<int>(grpc_status.error_code()));
    ir_write_status.mutable_rpc_wide_error()->set_message(
        grpc_status.error_message());
    return ir_write_status;
  } else if (grpc_status.error_code() == grpc::StatusCode::UNKNOWN &&
             !grpc_status.error_details().empty()) {
    google::rpc::Status inner_rpc_status;
    if (!inner_rpc_status.ParseFromString(grpc_status.error_details())) {
      return absl::InvalidArgumentError(
          "Can not parse error_details in grpc_status");
    }
    if (inner_rpc_status.code() != static_cast<int>(grpc_status.error_code())) {
      return gutil::InvalidArgumentErrorBuilder()
             << "google::rpc::Status's status code does not match "
                "with status code in grpc_status.";
    }

    auto *ir_rpc_response = ir_write_status.mutable_rpc_response();
    p4::v1::Error p4_error;
    bool all_p4_errors_ok = true;
    if (inner_rpc_status.details_size() != number_of_updates_in_write_request) {
      return gutil::InvalidArgumentErrorBuilder()
             << "Number of rpc status in google::rpc::status doesn't match "
                "number_of_update_in_write_request. inner_rpc_status: "
             << inner_rpc_status.details_size()
             << " number_of_updates_in_write_request: "
             << number_of_updates_in_write_request;
    }
    for (const auto &inner_rpc_status_detail : inner_rpc_status.details()) {
      if (!inner_rpc_status_detail.UnpackTo(&p4_error)) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Can not parse google::rpc::Status contained in grpc_status.";
      }
      RETURN_IF_ERROR(IsGoogleRpcCode(p4_error.canonical_code()));
      RETURN_IF_ERROR(ValidateGenericUpdateStatus(
          static_cast<google::rpc::Code>(p4_error.canonical_code()),
          p4_error.message()));
      if (p4_error.canonical_code() !=
          static_cast<int>(google::rpc::Code::OK)) {
        all_p4_errors_ok = false;
      }
      IrUpdateStatus *ir_update_status = ir_rpc_response->add_statuses();
      ir_update_status->set_code(
          static_cast<google::rpc::Code>(p4_error.canonical_code()));
      ir_update_status->set_message(p4_error.message());
    }
    if (all_p4_errors_ok) {
      return gutil::InvalidArgumentErrorBuilder()
             << "gRPC status should contain a mixure of successful and failed "
                "update status but all p4 errors are ok.";
    }
    return ir_write_status;

  } else {
    return gutil::InvalidArgumentErrorBuilder()
           << "Only rpc-wide error and batch update status formats are "
              "supported for non-ok gRPC status.";
  }
}

static absl::StatusOr<grpc::Status> IrWriteResponseToGrpcStatus(
    const IrWriteResponse &ir_write_response) {
  p4::v1::Error p4_error;
  google::rpc::Status inner_rpc_status;
  for (const IrUpdateStatus &ir_update_status : ir_write_response.statuses()) {
    RETURN_IF_ERROR(ValidateGenericUpdateStatus(ir_update_status.code(),
                                                ir_update_status.message()));
    RETURN_IF_ERROR(IsGoogleRpcCode(ir_update_status.code()));
    p4_error.set_canonical_code(static_cast<int>(ir_update_status.code()));
    p4_error.set_message(ir_update_status.message());
    inner_rpc_status.add_details()->PackFrom(p4_error);
  }
  inner_rpc_status.set_code(static_cast<int>(google::rpc::UNKNOWN));

  return grpc::Status(static_cast<grpc::StatusCode>(inner_rpc_status.code()),
                      IrWriteResponseToReadableMessage(ir_write_response),
                      inner_rpc_status.SerializeAsString());
}

absl::StatusOr<grpc::Status> IrWriteRpcStatusToGrpcStatus(
    const IrWriteRpcStatus &ir_write_status) {
  switch (ir_write_status.status_case()) {
    case IrWriteRpcStatus::kRpcResponse: {
      bool all_ir_update_status_ok =
          absl::c_all_of(ir_write_status.rpc_response().statuses(),
                         [](const IrUpdateStatus &ir_update_status) {
                           return ir_update_status.code() == google::rpc::OK;
                         });
      bool ir_update_status_has_no_error_message =
          absl::c_all_of(ir_write_status.rpc_response().statuses(),
                         [](const IrUpdateStatus &ir_update_status) {
                           return ir_update_status.message().empty();
                         });
      if (all_ir_update_status_ok && ir_update_status_has_no_error_message) {
        return grpc::Status(grpc::StatusCode::OK, "");
      } else {
        return IrWriteResponseToGrpcStatus(ir_write_status.rpc_response());
      }
    }
    case IrWriteRpcStatus::kRpcWideError: {
      RETURN_IF_ERROR(IsGoogleRpcCode(ir_write_status.rpc_wide_error().code()));
      if (ir_write_status.rpc_wide_error().code() ==
          static_cast<int>(google::rpc::Code::OK)) {
        return gutil::InvalidArgumentErrorBuilder()
               << "IR rpc-wide error should not have ok status.";
      }
      RETURN_IF_ERROR(ValidateGenericUpdateStatus(
          static_cast<google::rpc::Code>(
              ir_write_status.rpc_wide_error().code()),
          ir_write_status.rpc_wide_error().message()));
      return grpc::Status(static_cast<grpc::StatusCode>(
                              ir_write_status.rpc_wide_error().code()),
                          ir_write_status.rpc_wide_error().message());
    }
    case IrWriteRpcStatus::STATUS_NOT_SET:
      break;
  }
  return gutil::InvalidArgumentErrorBuilder()
         << "Invalid IrWriteRpcStatus: " << ir_write_status.DebugString();
}

absl::Status GrpcStatusToAbslStatus(const grpc::Status &grpc_status,
                                    int number_of_updates_in_write_request) {
  ASSIGN_OR_RETURN(IrWriteRpcStatus write_rpc_status,
                   GrpcStatusToIrWriteRpcStatus(
                       grpc_status, number_of_updates_in_write_request),
                   _ << "Invalid gRPC status w.r.t. P4RT specification: ");

  switch (write_rpc_status.status_case()) {
    case IrWriteRpcStatus::kRpcWideError: {
      return absl::Status(static_cast<absl::StatusCode>(
                              write_rpc_status.rpc_wide_error().code()),
                          write_rpc_status.rpc_wide_error().message());
    }
    case IrWriteRpcStatus::kRpcResponse: {
      const IrWriteResponse &ir_write_response =
          write_rpc_status.rpc_response();
      bool all_ir_update_status_ok =
          absl::c_all_of(ir_write_response.statuses(),
                         [](const IrUpdateStatus &ir_update_status) {
                           return ir_update_status.code() == google::rpc::OK;
                         });
      return (all_ir_update_status_ok)
                 ? absl::OkStatus()
                 : gutil::UnknownErrorBuilder()
                       << IrWriteResponseToReadableMessage(ir_write_response);
    }
    case IrWriteRpcStatus::STATUS_NOT_SET:
      break;
  }
  return gutil::InternalErrorBuilder()
         << "GrpcStatusToIrWriteRpcStatus returned invalid IrWriteRpcStatus: "
         << write_rpc_status.DebugString();
}

}  // namespace pdpi
