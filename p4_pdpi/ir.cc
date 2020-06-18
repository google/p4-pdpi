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

#include <sstream>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4_pdpi/util.h"

namespace pdpi {

using ::p4::config::v1::MatchField;
using ::p4::config::v1::P4TypeInfo;
using ::pdpi::ir::Format;
using ::pdpi::ir::IrActionDefinition;
using ::pdpi::ir::IrActionInvocation;
using ::pdpi::ir::IrMatchFieldDefinition;
using ::pdpi::ir::IrP4Info;
using ::pdpi::ir::IrTableDefinition;

namespace {

// Helper for GetFormat that extracts the necessary info from a P4Info element.
// T could be p4::config::v1::ControllerPacketMetadata::Metadata,
// p4::config::v1::MatchField, or p4::config::v1::Action::Param (basically
// anything that has a set of annotations, a bitwidth and named type
// information).
template <typename T>
StatusOr<Format> GetFormatForP4InfoElement(const T &element,
                                           const P4TypeInfo &type_info) {
  bool is_sdn_string = false;
  if (element.has_type_name()) {
    const auto &name = element.type_name().name();
    ASSIGN_OR_RETURN(
        const auto &named_type,
        FindElement(type_info.new_types(), name,
                    absl::StrCat("Missing type definition for ", name, ".")));
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
    google::protobuf::Map<uint32_t, ir::IrPacketIoMetadataDefinition> *by_id,
    google::protobuf::Map<std::string, ir::IrPacketIoMetadataDefinition>
        *by_name,
    const P4TypeInfo &type_info) {
  const std::string &kind = data.preamble().name();
  if (!by_id->empty()) {
    // Only checking by_id, since by_id->size() == by_name->size()
    return InvalidArgumentErrorBuilder()
           << "Found duplicate " << kind << " controller packet metadata.";
  }
  for (const auto &metadata : data.metadata()) {
    ir::IrPacketIoMetadataDefinition ir_metadata;
    ir_metadata.mutable_metadata()->CopyFrom(metadata);
    ASSIGN_OR_RETURN(const auto &format,
                     GetFormatForP4InfoElement(metadata, type_info));
    ir_metadata.set_format(format);
    RETURN_IF_ERROR(InsertIfUnique(
        by_id, metadata.id(), ir_metadata,
        absl::StrCat("Found several ", kind,
                     " metadata with the same ID: ", metadata.id(), ".")));
    RETURN_IF_ERROR(InsertIfUnique(
        by_name, metadata.name(), ir_metadata,
        absl::StrCat("Found several ", kind,
                     " metadata with the same name: ", metadata.name(), ".")));
  }
  return absl::OkStatus();
}

}  // namespace

StatusOr<std::unique_ptr<P4InfoManager>> P4InfoManager::Create(
    const p4::config::v1::P4Info &p4_info) {
  P4InfoManager p4info_manager;
  const P4TypeInfo &type_info = p4_info.type_info();

  // Translate all action definitions to IR.
  for (const auto &action : p4_info.actions()) {
    IrActionDefinition ir_action;
    ir_action.mutable_preamble()->CopyFrom(action.preamble());
    for (const auto &param : action.params()) {
      IrActionDefinition::IrActionParamDefinition ir_param;
      ir_param.mutable_param()->CopyFrom(param);
      ASSIGN_OR_RETURN(const auto &format,
                       GetFormatForP4InfoElement(param, type_info));
      ir_param.set_format(format);
      RETURN_IF_ERROR(InsertIfUnique(
          ir_action.mutable_params_by_id(), param.id(), ir_param,
          absl::StrCat("Found several parameters with the same ID ", param.id(),
                       " for action ", action.preamble().alias(), ".")));
      RETURN_IF_ERROR(InsertIfUnique(
          ir_action.mutable_params_by_name(), param.name(), ir_param,
          absl::StrCat("Found several parameters with the same name ",
                       param.id(), " for action ", action.preamble().alias(),
                       ".")));
    }
    RETURN_IF_ERROR(
        InsertIfUnique(p4info_manager.info_.mutable_actions_by_id(),
                       action.preamble().id(), ir_action,
                       absl::StrCat("Found several actions with the same ID: ",
                                    action.preamble().id(), ".")));
    RETURN_IF_ERROR(InsertIfUnique(
        p4info_manager.info_.mutable_actions_by_name(),
        action.preamble().alias(), ir_action,
        absl::StrCat("Found several actions with the same name: ",
                     action.preamble().id(), ".")));
  }

  // Translate all table definitions to IR.
  for (const auto &table : p4_info.tables()) {
    IrTableDefinition ir_table_definition;
    uint32_t table_id = table.preamble().id();
    ir_table_definition.mutable_preamble()->CopyFrom(table.preamble());
    for (const auto match_field : table.match_fields()) {
      IrMatchFieldDefinition ir_match_definition;
      ir_match_definition.mutable_match_field()->CopyFrom(match_field);
      ASSIGN_OR_RETURN(const auto &format,
                       GetFormatForP4InfoElement(match_field, type_info));
      ir_match_definition.set_format(format);

      RETURN_IF_ERROR(InsertIfUnique(
          ir_table_definition.mutable_match_fields_by_id(), match_field.id(),
          ir_match_definition,
          absl::StrCat("Found several match fields with the same ID ",
                       match_field.id(), " in table ", table.preamble().alias(),
                       ".")));
      RETURN_IF_ERROR(InsertIfUnique(
          ir_table_definition.mutable_match_fields_by_name(),
          match_field.name(), ir_match_definition,
          absl::StrCat("Found several match fields with the same name ",
                       match_field.name(), " in table ",
                       table.preamble().alias(), ".")));
      if (match_field.match_type() == MatchField::EXACT) {
        p4info_manager.num_mandatory_match_fields_[table_id] += 1;
      }
    }
    for (const auto &action_ref : table.action_refs()) {
      // Make sure the action is defined
      ASSIGN_OR_RETURN(
          *ir_table_definition.add_actions(),
          FindElement(p4info_manager.info_.actions_by_id(), action_ref.id(),
                      absl::StrCat("Missing definition for action with id ",
                                   action_ref.id(), ".")));
    }
    ir_table_definition.set_size(table.size());
    RETURN_IF_ERROR(
        InsertIfUnique(p4info_manager.info_.mutable_tables_by_id(), table_id,
                       ir_table_definition,
                       absl::StrCat("Found several tables with the same ID ",
                                    table.preamble().id(), ".")));
    RETURN_IF_ERROR(
        InsertIfUnique(p4info_manager.info_.mutable_tables_by_name(),
                       table.preamble().alias(), ir_table_definition,
                       absl::StrCat("Found several tables with the same name ",
                                    table.preamble().alias(), ".")));
  }

  // Validate and translate the packet-io metadata
  for (const auto &metadata : p4_info.controller_packet_metadata()) {
    const std::string &kind = metadata.preamble().name();
    if (kind == "packet_out") {
      RETURN_IF_ERROR(ProcessPacketIoMetadataDefinition(
          metadata, p4info_manager.info_.mutable_packet_out_metadata_by_id(),
          p4info_manager.info_.mutable_packet_out_metadata_by_name(),
          type_info));
    } else if (kind == "packet_in") {
      RETURN_IF_ERROR(ProcessPacketIoMetadataDefinition(
          metadata, p4info_manager.info_.mutable_packet_in_metadata_by_id(),
          p4info_manager.info_.mutable_packet_in_metadata_by_name(),
          type_info));
    } else {
      return InvalidArgumentErrorBuilder()
             << "Unknown controller packet metadata: " << kind
             << ". Only packet_in and packet_out are supported.";
    }
  }

  return absl::make_unique<P4InfoManager>(p4info_manager);
}

IrP4Info P4InfoManager::GetIrP4Info() const { return info_; }

StatusOr<IrTableDefinition> P4InfoManager::GetIrTableDefinition(
    uint32_t table_id) const {
  return FindElement(
      info_.tables_by_id(), table_id,
      absl::StrCat("Table with ID ", table_id, " does not exist."));
}

StatusOr<IrActionDefinition> P4InfoManager::GetIrActionDefinition(
    uint32_t action_id) const {
  return FindElement(
      info_.actions_by_id(), action_id,
      absl::StrCat("Action with ID ", action_id, " does not exist."));
}

namespace {
// Verifies the contents of the PI representation and translates to the IR
// message
StatusOr<ir::IrMatch> PiMatchFieldToIr(
    const IrMatchFieldDefinition &ir_match_definition,
    const p4::v1::FieldMatch &pi_match) {
  ir::IrMatch match_entry;
  const MatchField &match_field = ir_match_definition.match_field();
  uint32_t bitwidth = match_field.bitwidth();
  match_entry.set_name(match_field.name());

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!pi_match.has_exact()) {
        return InvalidArgumentErrorBuilder()
               << "Expected exact match type in PI.";
      }

      ASSIGN_OR_RETURN(*match_entry.mutable_exact(),
                       FormatByteString(ir_match_definition.format(), bitwidth,
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

      if (ir_match_definition.format() != Format::IPV4 &&
          ir_match_definition.format() != Format::IPV6) {
        return InvalidArgumentErrorBuilder()
               << "LPM is supported only for " << Format::IPV4 << " and "
               << Format::IPV6 << " formats. Got "
               << ir_match_definition.format() << " instead.";
      }
      match_entry.mutable_lpm()->set_prefix_length(prefix_len);
      ASSIGN_OR_RETURN(*match_entry.mutable_lpm()->mutable_value(),
                       FormatByteString(ir_match_definition.format(), bitwidth,
                                        pi_match.lpm().value()));
      break;
    }
    case MatchField::TERNARY: {
      if (!pi_match.has_ternary()) {
        return InvalidArgumentErrorBuilder()
               << "Expected Ternary match type in PI.";
      }

      ASSIGN_OR_RETURN(*match_entry.mutable_ternary()->mutable_value(),
                       FormatByteString(ir_match_definition.format(), bitwidth,
                                        pi_match.ternary().value()));
      ASSIGN_OR_RETURN(*match_entry.mutable_ternary()->mutable_mask(),
                       FormatByteString(ir_match_definition.format(), bitwidth,
                                        pi_match.ternary().mask()));
      break;
    }
    default:
      return InvalidArgumentErrorBuilder()
             << "Unsupported match type "
             << MatchField_MatchType_Name(match_field.match_type()) << " in "
             << match_entry.name() << ".";
  }
  return match_entry;
}
}  // namespace

StatusOr<IrActionInvocation> P4InfoManager::PiActionInvocationToIr(
    const p4::v1::TableAction &pi_table_action,
    const google::protobuf::RepeatedPtrField<IrActionDefinition> &valid_actions)
    const {
  IrActionInvocation action_entry;
  switch (pi_table_action.type_case()) {
    case p4::v1::TableAction::kAction: {
      const auto pi_action = pi_table_action.action();
      uint32_t action_id = pi_action.action_id();

      ASSIGN_OR_RETURN(const auto &ir_action_definition,
                       FindElement(info_.actions_by_id(), action_id,
                                   absl::StrCat("Action ID ", action_id,
                                                " missing in P4Info.")));

      if (absl::c_find_if(valid_actions,
                          [action_id](const IrActionDefinition &action) {
                            return action.preamble().id() == action_id;
                          }) == valid_actions.end()) {
        return InvalidArgumentErrorBuilder()
               << "Action ID " << action_id << " is not a valid action.";
      }

      int action_params_size = ir_action_definition.params_by_id().size();
      if (action_params_size != pi_action.params().size()) {
        return InvalidArgumentErrorBuilder()
               << "Expected " << action_params_size << " parameters, but got "
               << pi_action.params().size() << " instead in action with ID "
               << action_id << ".";
      }
      action_entry.set_name(ir_action_definition.preamble().alias());
      for (const auto &param : pi_action.params()) {
        absl::flat_hash_set<uint32_t> used_params;
        RETURN_IF_ERROR(
            InsertIfUnique(used_params, param.param_id(),
                           absl::StrCat("Duplicate param field found with ID ",
                                        param.param_id(), ".")));

        ASSIGN_OR_RETURN(
            const auto &ir_param_definition,
            FindElement(
                ir_action_definition.params_by_id(), param.param_id(),
                absl::StrCat("Unable to find param ID ", param.param_id(),
                             " in action with ID ", action_id)));
        IrActionInvocation::IrActionParam *param_entry =
            action_entry.add_params();
        param_entry->set_name(ir_param_definition.param().name());
        ASSIGN_OR_RETURN(
            *param_entry->mutable_value(),
            FormatByteString(ir_param_definition.format(),
                             ir_param_definition.param().bitwidth(),
                             param.value()));
      }
      break;
    }
    default:
      return InvalidArgumentErrorBuilder()
             << "Unsupported action type: " << pi_table_action.type_case();
  }
  return action_entry;
}

StatusOr<ir::IrTableEntry> P4InfoManager::PiTableEntryToIr(
    const p4::v1::TableEntry &pi) const {
  ir::IrTableEntry ir;
  ASSIGN_OR_RETURN(const auto &table,
                   FindElement(info_.tables_by_id(), pi.table_id(),
                               absl::StrCat("Table ID ", pi.table_id(),
                                            " missing in P4Info.")));
  ir.set_table_name(table.preamble().alias());

  // Validate and translate the matches
  absl::flat_hash_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  for (const auto pi_match : pi.match()) {
    RETURN_IF_ERROR(
        InsertIfUnique(used_field_ids, pi_match.field_id(),
                       absl::StrCat("Duplicate match field found with ID ",
                                    pi_match.field_id(), ".")));

    ASSIGN_OR_RETURN(
        const auto &match,
        FindElement(table.match_fields_by_id(), pi_match.field_id(),
                    absl::StrCat("Match Field ", pi_match.field_id(),
                                 " missing in table ", ir.table_name(), ".")));
    ASSIGN_OR_RETURN(const auto &match_entry,
                     PiMatchFieldToIr(match, pi_match));
    ir.add_matches()->CopyFrom(match_entry);

    if (match.match_field().match_type() == MatchField::EXACT) {
      ++mandatory_matches;
    }
  }

  ASSIGN_OR_RETURN(int expected_mandatory_matches,
                   FindElement(num_mandatory_match_fields_, pi.table_id(),
                               "Table not found."));
  if (mandatory_matches != expected_mandatory_matches) {
    return InvalidArgumentErrorBuilder()
           << "Expected " << expected_mandatory_matches
           << " mandatory match conditions but found " << mandatory_matches
           << " instead.";
  }

  // Validate and translate the action.
  if (!pi.has_action()) {
    return InvalidArgumentErrorBuilder()
           << "Action missing in TableEntry with ID " << pi.table_id() << ".";
  }
  ASSIGN_OR_RETURN(const auto &action_entry,
                   PiActionInvocationToIr(pi.action(), table.actions()));
  ir.mutable_action()->CopyFrom(action_entry);

  return ir;
}

template <typename I, typename O>
StatusOr<O> P4InfoManager::PiPacketIoToIr(const std::string &kind,
                                          const I &packet) const {
  O result;
  result.set_payload(packet.payload());
  absl::flat_hash_set<uint32_t> used_metadata_ids;
  for (const auto &metadata : packet.metadata()) {
    uint32_t id = metadata.metadata_id();
    RETURN_IF_ERROR(InsertIfUnique(
        used_metadata_ids, id,
        absl::StrCat("Duplicate ", kind, " metadata found with ID ", id, ".")));

    ASSIGN_OR_RETURN(const auto &metadata_definition,
                     FindElement(info_.packet_in_metadata_by_id(), id,
                                 absl::StrCat(kind, " metadata with ID ", id,
                                              " not defined.")));
    ir::IrPacketMetadata ir_metadata;
    ir_metadata.set_name(metadata_definition.metadata().name());
    ASSIGN_OR_RETURN(*ir_metadata.mutable_value(),
                     FormatByteString(metadata_definition.format(),
                                      metadata_definition.metadata().bitwidth(),
                                      metadata.value()));
    *result.add_metadata() = ir_metadata;
  }
  // Check for missing metadata
  for (const auto &item : info_.packet_in_metadata_by_id()) {
    const auto& id = item.first;
    const auto& meta = item.second;
    if (!used_metadata_ids.contains(id)) {
      return InvalidArgumentErrorBuilder()
             << kind << " metadata " << meta.metadata().name() << " with ID "
             << id << " is missing.";
    }
  }

  return result;
}

StatusOr<ir::IrPacketIn> P4InfoManager::PiPacketInToIr(
    const p4::v1::PacketIn &packet) const {
  return PiPacketIoToIr<p4::v1::PacketIn, ir::IrPacketIn>("packet-in", packet);
}
StatusOr<ir::IrPacketOut> P4InfoManager::PiPacketOutToIr(
    const p4::v1::PacketOut &packet) const {
  return PiPacketIoToIr<p4::v1::PacketOut, ir::IrPacketOut>("packet-out",
                                                            packet);
}

template <typename I, typename O>
StatusOr<I> P4InfoManager::IrPacketIoToPi(const std::string &kind,
                                          const O &packet) const {
  I result;
  result.set_payload(packet.payload());
  absl::flat_hash_set<std::string> used_metadata_names;
  for (const auto &metadata : packet.metadata()) {
    const std::string &name = metadata.name();
    RETURN_IF_ERROR(
        InsertIfUnique(used_metadata_names, name,
                       absl::StrCat("Duplicate ", kind,
                                    " metadata found with name ", name, ".")));

    ASSIGN_OR_RETURN(const auto &metadata_definition,
                     FindElement(info_.packet_in_metadata_by_name(), name,
                                 absl::StrCat(kind, " metadata with name ",
                                              name, " not defined.")));
    p4::v1::PacketMetadata pi_metadata;
    pi_metadata.set_metadata_id(metadata_definition.metadata().id());
    ASSIGN_OR_RETURN(auto value, IrValueToByteString(metadata.value()));
    pi_metadata.set_value(value);
    *result.add_metadata() = pi_metadata;
  }
  // Check for missing metadata
  for (const auto &item : info_.packet_in_metadata_by_name()) {
    const auto& name = item.first;
    const auto& meta = item.second;
    if (!used_metadata_names.contains(name)) {
      return InvalidArgumentErrorBuilder()
             << kind << " metadata " << meta.metadata().name() << " with id "
             << meta.metadata().id() << " is missing.";
    }
  }

  return result;
}

StatusOr<p4::v1::PacketIn> P4InfoManager::IrPacketInToPi(
    const ir::IrPacketIn &packet) const {
  return IrPacketIoToPi<p4::v1::PacketIn, ir::IrPacketIn>("packet-in", packet);
}
StatusOr<p4::v1::PacketOut> P4InfoManager::IrPacketOutToPi(
    const ir::IrPacketOut &packet) const {
  return IrPacketIoToPi<p4::v1::PacketOut, ir::IrPacketOut>("packet-out",
                                                            packet);
}

}  // namespace pdpi
