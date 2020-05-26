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

#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"
#include "src/util.h"

namespace pdpi {
using ::p4::config::v1::MatchField;
using ::pdpi::ir::IrAction;

P4InfoManager::P4InfoManager(const p4::config::v1::P4Info &p4_info)
    : p4_metadata_(CreateMetadata(p4_info)) {}

// Verifies the contents of the PI representation and translates to the IR
// message
static pdpi::ir::IrMatch PiMatchFieldToIr(
    const P4MatchFieldMetadata &match_metadata,
    const p4::v1::FieldMatch &pi_match) {
  pdpi::ir::IrMatch match_entry;
  const MatchField &match_field = match_metadata.match_field;
  uint32_t bitwidth = match_field.bitwidth();
  match_entry.set_name(match_field.name());

  switch (match_field.match_type()) {
    case MatchField::EXACT: {
      if (!pi_match.has_exact()) {
        throw std::invalid_argument("Expected exact match type in PI.");
      }

      match_entry.set_exact(FormatByteString(match_metadata.format, bitwidth,
                                             pi_match.exact().value()));
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

      if (match_metadata.format != Format::IPV4 &&
          match_metadata.format != Format::IPV6) {
        throw std::invalid_argument(absl::StrCat(
            "LPM is supported only for ", Format::IPV4, " and ", Format::IPV6,
            " formats. ", "Got ", match_metadata.format, " instead."));
      }
      match_entry.set_lpm(absl::StrCat(
          FormatByteString(match_metadata.format, bitwidth,
                           Normalize(pi_match.lpm().value(), bitwidth)),
          "/", prefix_len));
      break;
    }
    case MatchField::TERNARY: {
      if (!pi_match.has_ternary()) {
        throw std::invalid_argument(
            absl::StrCat("Expected Ternary match ", "type in PI."));
      }

      match_entry.mutable_ternary()->set_value(
          FormatByteString(match_metadata.format, bitwidth,
                           Normalize(pi_match.ternary().value(), bitwidth)));
      match_entry.mutable_ternary()->set_mask(
          FormatByteString(match_metadata.format, bitwidth,
                           Normalize(pi_match.ternary().mask(), bitwidth)));

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

IrAction P4InfoManager::PiActionToIr(
    const p4::v1::TableAction &pi_table_action,
    const absl::flat_hash_set<uint32_t> &valid_actions) {
  IrAction action_entry;
  switch (pi_table_action.type_case()) {
    case p4::v1::TableAction::kAction: {
      const auto pi_action = pi_table_action.action();
      uint32_t action_id = pi_action.action_id();

      const auto &action_metadata = FindElement(
          p4_metadata_.actions, action_id,
          absl::StrCat("Action ID ", action_id, " missing in metadata."));

      if (valid_actions.find(action_id) == valid_actions.end()) {
        throw std::invalid_argument(
            absl::StrCat("Action ID ", action_id, " is not a valid action."));
      }

      int action_params_size = action_metadata.params.size();
      if (action_params_size != pi_action.params().size()) {
        throw std::invalid_argument(
            absl::StrCat("Expected ", action_params_size,
                         " parameters, but got ", pi_action.params().size(),
                         " instead in action with ID ", action_id, "."));
      }
      action_entry.set_name(action_metadata.preamble.alias());
      for (const auto &param : pi_action.params()) {
        absl::flat_hash_set<uint32_t> used_params;
        InsertIfUnique(used_params, param.param_id(),
                       absl::StrCat("Duplicate param field found with ID ",
                                    param.param_id(), "."));

        const auto &param_metadata = FindElement(
            action_metadata.params, param.param_id(),
            absl::StrCat("Unable to find param ID ", param.param_id(),
                         " in action with ID ", action_id));
        IrAction::IrActionParam *param_entry = action_entry.add_params();
        param_entry->set_name(param_metadata.param.name());
        param_entry->set_value(FormatByteString(param_metadata.format,
                                                param_metadata.param.bitwidth(),
                                                param.value()));
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
    const p4::v1::TableEntry &pi) {
  pdpi::ir::IrTableEntry ir;
  // Collect table info from metadata
  const auto &table = FindElement(
      p4_metadata_.tables, pi.table_id(),
      absl::StrCat("Table ID ", pi.table_id(), " missing in metadata."));
  ir.set_table_name(table.preamble.alias());

  // Validate and translate the matches
  absl::flat_hash_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  for (const auto pi_match : pi.match()) {
    InsertIfUnique(used_field_ids, pi_match.field_id(),
                   absl::StrCat("Duplicate match field found with ID ",
                                pi_match.field_id(), "."));

    const auto &match =
        FindElement(table.match_fields, pi_match.field_id(),
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

    if (match.match_field.match_type() == MatchField::EXACT) {
      ++mandatory_matches;
    }
  }

  int expected_mandatory_matches = table.num_mandatory_match_fields;
  if (mandatory_matches != expected_mandatory_matches) {
    throw std::invalid_argument(absl::StrCat(
        "Expected ", expected_mandatory_matches, " mandatory match conditions ",
        "but found ", mandatory_matches, " instead."));
  }

  // Validate and translate the action
  if (!pi.has_action()) {
    throw std::invalid_argument(absl::StrCat(
        "Action missing in ", "TableEntry with ID ", pi.table_id()));
  }
  const auto action_entry = PiActionToIr(pi.action(), table.valid_actions);
  ir.mutable_action()->CopyFrom(action_entry);

  return ir;
}
}  // namespace pdpi
