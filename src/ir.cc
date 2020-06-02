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

#include "src/ir.h"

#include <sstream>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"
#include "src/util.h"

namespace pdpi {

using ::p4::config::v1::MatchField;
using ::pdpi::ir::Format;
using ::pdpi::ir::IrActionDefinition;
using ::pdpi::ir::IrActionInvocation;
using ::pdpi::ir::IrMatchFieldDefinition;
using ::pdpi::ir::IrP4Info;
using ::pdpi::ir::IrTableDefinition;

P4InfoManager::P4InfoManager(const p4::config::v1::P4Info &p4_info) {
  // Translate all action definitions to IR.
  for (const auto &action : p4_info.actions()) {
    IrActionDefinition ir_action;
    ir_action.mutable_preamble()->CopyFrom(action.preamble());
    for (const auto &param : action.params()) {
      IrActionDefinition::IrActionParamDefinition ir_param;
      ir_param.mutable_param()->CopyFrom(param);

      std::vector<std::string> annotations;
      for (const auto &annotation : param.annotations()) {
        annotations.push_back(annotation);
      }
      absl::optional<std::string> named_type;
      if (param.has_type_name()) {
        named_type = param.type_name().name();
      }
      ir_param.set_format(GetFormat(annotations, param.bitwidth(), named_type));
      InsertIfUnique(
          ir_action.mutable_params_by_id(), param.id(), ir_param,
          absl::StrCat("Found several parameters with the same ID ", param.id(),
                       " for action ", action.preamble().alias(), "."));
      InsertIfUnique(
          ir_action.mutable_params_by_name(), param.name(), ir_param,
          absl::StrCat("Found several parameters with the same name ",
                       param.id(), " for action ", action.preamble().alias(),
                       "."));
    }
    InsertIfUnique(info_.mutable_actions_by_id(), action.preamble().id(), ir_action,
                   absl::StrCat("Found several actions with the same ID: ",
                                action.preamble().id(), "."));
    InsertIfUnique(info_.mutable_actions_by_name(), action.preamble().alias(),
                   ir_action,
                   absl::StrCat("Found several actions with the same name: ",
                                action.preamble().id(), "."));
  }

  // Translate all table definitions to IR.
  for (const auto &table : p4_info.tables()) {
    IrTableDefinition ir_table_definition;
    uint32_t table_id = table.preamble().id();
    ir_table_definition.mutable_preamble()->CopyFrom(table.preamble());
    for (const auto match_field : table.match_fields()) {
      IrMatchFieldDefinition ir_match_definition;
      std::vector<std::string> annotations;
      for (const auto &annotation : match_field.annotations()) {
        annotations.push_back(annotation);
      }
      absl::optional<std::string> named_type;
      if (match_field.has_type_name()) {
        named_type = match_field.type_name().name();
      }
      ir_match_definition.mutable_match_field()->CopyFrom(match_field);
      ir_match_definition.set_format(
          GetFormat(annotations, match_field.bitwidth(), named_type));

      InsertIfUnique(
          ir_table_definition.mutable_match_fields_by_id(), match_field.id(),
          ir_match_definition,
          absl::StrCat("Found several match fields with the same ID ",
                       match_field.id(), " in table ", table.preamble().alias(),
                       "."));
      InsertIfUnique(
          ir_table_definition.mutable_match_fields_by_name(),
          match_field.name(), ir_match_definition,
          absl::StrCat("Found several match fields with the same name ",
                       match_field.name(), " in table ",
                       table.preamble().alias(), "."));
      if (match_field.match_type() == MatchField::EXACT) {
        num_mandatory_match_fields_[table_id] += 1;
      }
    }
    for (const auto &action_ref : table.action_refs()) {
      // Make sure the action is defined
      const auto action =
          FindElement(info_.actions_by_id(), action_ref.id(),
                      absl::StrCat("Missing definition for action with id ",
                                   action_ref.id(), "."));
      *ir_table_definition.add_actions() = action;
    }
    ir_table_definition.set_size(table.size());
    InsertIfUnique(info_.mutable_tables_by_id(), table_id, ir_table_definition,
                   absl::StrCat("Found several tables with the same ID ",
                                table.preamble().id(), "."));
    InsertIfUnique(info_.mutable_tables_by_name(), table.preamble().alias(),
                   ir_table_definition,
                   absl::StrCat("Found several tables with the same name ",
                                table.preamble().alias(), "."));
  }
}

IrP4Info P4InfoManager::GetIrP4Info() const { return info_; }

IrTableDefinition P4InfoManager::GetIrTableDefinition(uint32_t table_id) const {
  return FindElement(
      info_.tables_by_id(), table_id,
      absl::StrCat("Table with ID ", table_id, " does not exist."));
}

IrActionDefinition P4InfoManager::GetIrActionDefinition(
    uint32_t action_id) const {
  return FindElement(
      info_.actions_by_id(), action_id,
      absl::StrCat("Action with ID ", action_id, " does not exist."));
}

// Verifies the contents of the PI representation and translates to the IR
// message
static pdpi::ir::IrMatch PiMatchFieldToIr(
    const IrMatchFieldDefinition &ir_match_definition,
    const p4::v1::FieldMatch &pi_match) {
  pdpi::ir::IrMatch match_entry;
  const MatchField &match_field = ir_match_definition.match_field();
  uint32_t bitwidth = match_field.bitwidth();
  match_entry.set_name(match_field.name());

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!pi_match.has_exact()) {
        throw std::invalid_argument("Expected exact match type in PI.");
      }

      *match_entry.mutable_exact() = FormatByteString(
          ir_match_definition.format(), bitwidth, pi_match.exact().value());
      break;
    }
    case MatchField::LPM: {
      if (!pi_match.has_lpm()) {
        throw std::invalid_argument(
            absl::StrCat("Expected LPM match type ", "in PI."));
      }

      uint32_t prefix_len = pi_match.lpm().prefix_len();
      if (prefix_len > bitwidth) {
        throw std::invalid_argument(absl::StrCat("Prefix length ", prefix_len,
                                                 " is greater than bitwidth ",
                                                 bitwidth, " in LPM."));
      }

      if (ir_match_definition.format() != Format::IPV4 &&
          ir_match_definition.format() != Format::IPV6) {
        throw std::invalid_argument(absl::StrCat(
            "LPM is supported only for ", Format::IPV4, " and ", Format::IPV6,
            " formats. ", "Got ", ir_match_definition.format(), " instead."));
      }
      match_entry.mutable_lpm()->set_prefix_length(prefix_len);
      *match_entry.mutable_lpm()->mutable_value() = FormatByteString(
          ir_match_definition.format(), bitwidth, pi_match.lpm().value());
      break;
    }
    case MatchField::TERNARY: {
      if (!pi_match.has_ternary()) {
        throw std::invalid_argument(
            absl::StrCat("Expected Ternary match ", "type in PI."));
      }

      *match_entry.mutable_ternary()->mutable_value() =
          FormatByteString(ir_match_definition.format(), bitwidth,
                           Normalize(pi_match.ternary().value(), bitwidth));
      *match_entry.mutable_ternary()->mutable_mask() =
          FormatByteString(ir_match_definition.format(), bitwidth,
                           Normalize(pi_match.ternary().mask(), bitwidth));

      break;
    }
    default:
      throw std::invalid_argument(
          absl::StrCat("Unsupported match type ",
                       MatchField_MatchType_Name(match_field.match_type()),
                       " in ", match_entry.name(), "."));
  }
  return match_entry;
}

IrActionInvocation P4InfoManager::PiActionInvocationToIr(
    const p4::v1::TableAction &pi_table_action,
    const google::protobuf::RepeatedPtrField<IrActionDefinition> &valid_actions)
    const {
  IrActionInvocation action_entry;
  switch (pi_table_action.type_case()) {
    case p4::v1::TableAction::kAction: {
      const auto pi_action = pi_table_action.action();
      uint32_t action_id = pi_action.action_id();

      const auto &ir_action_definition = FindElement(
          info_.actions_by_id(), action_id,
          absl::StrCat("Action ID ", action_id, " missing in P4Info."));

      if (absl::c_find_if(valid_actions,
                          [action_id](const IrActionDefinition &action) {
                            return action.preamble().id() == action_id;
                          }) == valid_actions.end()) {
        throw std::invalid_argument(
            absl::StrCat("Action ID ", action_id, " is not a valid action."));
      }

      int action_params_size = ir_action_definition.params_by_id().size();
      if (action_params_size != pi_action.params().size()) {
        throw std::invalid_argument(
            absl::StrCat("Expected ", action_params_size,
                         " parameters, but got ", pi_action.params().size(),
                         " instead in action with ID ", action_id, "."));
      }
      action_entry.set_name(ir_action_definition.preamble().alias());
      for (const auto &param : pi_action.params()) {
        absl::flat_hash_set<uint32_t> used_params;
        InsertIfUnique(used_params, param.param_id(),
                       absl::StrCat("Duplicate param field found with ID ",
                                    param.param_id(), "."));

        const auto &ir_param_definition = FindElement(
            ir_action_definition.params_by_id(), param.param_id(),
            absl::StrCat("Unable to find param ID ", param.param_id(),
                         " in action with ID ", action_id));
        IrActionInvocation::IrActionParam *param_entry =
            action_entry.add_params();
        param_entry->set_name(ir_param_definition.param().name());
        *param_entry->mutable_value() = FormatByteString(
            ir_param_definition.format(),
            ir_param_definition.param().bitwidth(), param.value());
      }
      break;
    }
    default:
      throw std::invalid_argument(absl::StrCat("Unsupported action type: ",
                                               pi_table_action.type_case()));
  }
  return action_entry;
}

pdpi::ir::IrTableEntry P4InfoManager::PiTableEntryToIr(
    const p4::v1::TableEntry &pi) const {
  pdpi::ir::IrTableEntry ir;
  const auto &table = FindElement(
      info_.tables_by_id(), pi.table_id(),
      absl::StrCat("Table ID ", pi.table_id(), " missing in P4Info."));
  ir.set_table_name(table.preamble().alias());

  // Validate and translate the matches
  absl::flat_hash_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  for (const auto pi_match : pi.match()) {
    InsertIfUnique(used_field_ids, pi_match.field_id(),
                   absl::StrCat("Duplicate match field found with ID ",
                                pi_match.field_id(), "."));

    const auto &match =
        FindElement(table.match_fields_by_id(), pi_match.field_id(),
                    absl::StrCat("Match Field ", pi_match.field_id(),
                                 " missing in table ", ir.table_name(), "."));
    try {
      const auto match_entry = PiMatchFieldToIr(match, pi_match);
      ir.add_matches()->CopyFrom(match_entry);
    } catch (const std::invalid_argument &e) {
      throw std::invalid_argument(
          absl::StrCat("Could not convert the match ", "field with ID ",
                       pi_match.field_id(), ": ", e.what()));
    }

    if (match.match_field().match_type() == MatchField::EXACT) {
      ++mandatory_matches;
    }
  }

  int expected_mandatory_matches = FindElement(
      num_mandatory_match_fields_, pi.table_id(), "Table not found.");
  if (mandatory_matches != expected_mandatory_matches) {
    throw std::invalid_argument(absl::StrCat(
        "Expected ", expected_mandatory_matches, " mandatory match conditions ",
        "but found ", mandatory_matches, " instead."));
  }

  // Validate and translate the action.
  if (!pi.has_action()) {
    throw std::invalid_argument(absl::StrCat(
        "Action missing in ", "TableEntry with ID ", pi.table_id()));
  }
  const auto action_entry =
      PiActionInvocationToIr(pi.action(), table.actions());
  ir.mutable_action()->CopyFrom(action_entry);

  return ir;
}

}  // namespace pdpi
