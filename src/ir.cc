#include "src/ir.h"

#include <sstream>

#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"

#include "src/util.h"

namespace pdpi {
using ::p4::config::v1::MatchField;

// Verifies the contents of the PI representation and translates to the PD
// message
void PiMatchFieldToIr(const P4MatchFieldMetadata &match_metadata,
                      const p4::v1::FieldMatch &pi_match,
                      IrMatch *ir_match) {
  const MatchField &match_field = match_metadata.match_field;
  uint32_t bitwidth = match_field.bitwidth();
  ir_match->name = match_field.name();

  switch (match_field.match_type()) {
    case MatchField::EXACT:
      {
        if (!pi_match.has_exact()) {
          throw std::invalid_argument("Expected exact match type in PI.");
        }

        ir_match->value = FormatByteString(match_metadata.format,
                                           bitwidth,
                                           pi_match.exact().value());
        break;
      }
    case MatchField::LPM:
      {
        if (!pi_match.has_lpm()) {
          throw std::invalid_argument(absl::StrCat("Expected LPM match type ",
                                                   "in PI."));
        }

        uint32_t prefix_len = pi_match.lpm().prefix_len();
        if (prefix_len > bitwidth) {
          throw std::invalid_argument(absl::StrCat("Prefix length ",
                                                   prefix_len,
                                                   " is greater than bitwidth ",
                                                   bitwidth, " in LPM."));
        }

        if (match_metadata.format != Format::IPV4 &&
            match_metadata.format != Format::IPV6) {
          throw std::invalid_argument(absl::StrCat(
              "LPM is supported only for ", Format::IPV4, " and ", Format::IPV6,
              " formats. ", "Got ", match_metadata.format, " instead."));
        }
        ir_match->value = absl::StrCat(
                              FormatByteString(match_metadata.format,
                                               bitwidth,
                                               Normalize(pi_match.lpm().value(),
                                                         bitwidth)),
                              "/", prefix_len);
        break;
      }
    case MatchField::TERNARY:
      {
        if (!pi_match.has_ternary()) {
          throw std::invalid_argument(absl::StrCat("Expected Ternary match ",
                                                   "type in PI."));
        }

        IrTernaryMatch ternary_match;
        ternary_match.value = FormatByteString(
                                  match_metadata.format,
                                  bitwidth,
                                  Normalize(pi_match.ternary().value(),
                                            bitwidth));
        ternary_match.mask = FormatByteString(
                                match_metadata.format,
                                bitwidth,
                                Normalize(pi_match.ternary().mask(),
                                          bitwidth));

        ir_match->value = ternary_match;
        break;
      }
    default:
      throw std::invalid_argument(
          absl::StrCat("Unsupported match type ",
                       MatchField_MatchType_Name(
                           match_field.match_type()),
                       " in ", ir_match->name, "."));
  }
}

// Translates all matches from their PI form to IR
void PiMatchesToIr(const P4TableMetadata &table_metadata,
                   const p4::v1::TableEntry &pi,
                   IrTableEntry *ir) {
  absl::flat_hash_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  for (const auto pi_match : pi.match()) {
    InsertIfUnique(used_field_ids,
                   pi_match.field_id(),
                   absl::StrCat("Duplicate match field found with ID ",
                                pi_match.field_id(), "."));

    const auto &match_metadata = FindElement(table_metadata.match_fields,
                                             pi_match.field_id(),
                                             absl::StrCat(
                                                 "Match Field ",
                                                 pi_match.field_id(),
                                                 " missing in table ",
                                                 ir->table_name,
                                                 "."));
    IrMatch ir_match;
    try {
      PiMatchFieldToIr(match_metadata,
                       pi_match,
                       &ir_match);
      ir->matches.push_back(ir_match);
    } catch (const std::invalid_argument& e) {
      throw std::invalid_argument(absl::StrCat("Could not convert the match ",
                                               "field with ID ",
                                               pi_match.field_id(), ": ",
                                               e.what()));
    }

    if (match_metadata.match_field.match_type() == MatchField::EXACT) {
      ++mandatory_matches;
    }
  }

  int expected_mandatory_matches = table_metadata.num_mandatory_match_fields;
  if (mandatory_matches != expected_mandatory_matches) {
    throw std::invalid_argument(absl::StrCat("Expected ",
                                             expected_mandatory_matches,
                                             " mandatory match conditions ",
                                             "but found ", mandatory_matches,
                                             " instead."));
  }
}

// Translates the action from its PI form to IR
void PiActionToIr(const P4InfoMetadata &metadata,
                  const p4::v1::TableEntry &pi,
                  IrTableEntry *ir) {
  const auto &table = FindElement(metadata.tables,
                                  pi.table_id(),
                                  absl::StrCat("Table ID ", pi.table_id(),
                                               " missing in metadata."));
  if (!pi.has_action()) {
    throw std::invalid_argument(absl::StrCat("Action missing in ",
                                             "TableEntry with ID ",
                                             pi.table_id()));
  }
  switch (pi.action().type_case()) {
    case p4::v1::TableAction::kAction:
      {
        const auto pi_action = pi.action().action();
        uint32_t action_id = pi_action.action_id();

        const auto &action_metadata = FindElement(metadata.actions,
                                                  action_id,
                                                  absl::StrCat(
                                                      "Action ID ", action_id,
                                                      " missing in metadata."));

        if (table.valid_actions.find(action_id) == table.valid_actions.end()) {
          throw std::invalid_argument(absl::StrCat("Action ID ", action_id,
                                                   " is not a valid action ",
                                                   " for table with table ID",
                                                   pi.table_id(), "."));
        }

        int action_params_size = action_metadata.params.size();
        if (action_params_size != pi_action.params().size()) {
          throw std::invalid_argument(absl::StrCat("Expected ",
                                                   action_params_size,
                                                   " parameters, but got ",
                                                   pi_action.params().size(),
                                                   " instead in action with ID ",
                                                   action_id, "."));
        }
        IrAction action_entry;
        action_entry.name = action_metadata.preamble.alias();
        for (const auto &param : pi_action.params()) {
          absl::flat_hash_set<uint32_t> used_params;
          InsertIfUnique(used_params,
                         param.param_id(),
                         absl::StrCat("Duplicate param field found with ID ",
                                      param.param_id(), "."));

          const auto &param_metadata = FindElement(action_metadata.params,
                                                   param.param_id(),
                                                   absl::StrCat(
                                                       "Unable to find param ID ",
                                                       param.param_id(),
                                                       " in action with ID ",
                                                       action_id));
          IrActionParam param_entry;
          param_entry.name = param_metadata.param.name();
          param_entry.value = FormatByteString(param_metadata.format,
                                               param_metadata.param.bitwidth(),
                                               param.value());
          action_entry.params.push_back(param_entry);
        }

        ir->action.emplace(action_entry);
        break;
      }
    default:
      throw std::invalid_argument(absl::StrCat("Unsupported action type: ",
                                               pi.action().type_case()));
  }
}

IrTableEntry PiToIr(const P4InfoMetadata &metadata,
                    const p4::v1::TableEntry& pi) {
  IrTableEntry ir;
  const auto &table = FindElement(metadata.tables, pi.table_id(),
                                  absl::StrCat("Table ID ", pi.table_id(),
                                  " missing in metadata."));
  ir.table_name = table.preamble.alias();

  PiMatchesToIr(table, pi, &ir);
  PiActionToIr(metadata, pi, &ir);

  return ir;
}

std::string IrToString(const IrTableEntry& ir) {
  std::stringstream ss;
  std::string indent = "  ";
  ss << "table_name: \"" << EscapeString(ir.table_name)
     << "\"" << std::endl;
  for (const IrMatch& match : ir.matches) {
    ss << indent << "match {" << std::endl;
    ss << indent << indent << "name: \""
       << EscapeString(match.name) << "\"" << std::endl;
    ss << indent << indent << "value: {" << std::endl;
    switch (match.value.index()) {
      case 0:
        ss << indent << indent << indent << "string: \""
           << EscapeString(absl::get<0>(match.value)) << "\"" << std::endl;
        break;
      case 1:
        ss << indent << indent << indent << "ternary: {" << std::endl;
        ss << indent << indent << indent << indent << "value: \""
           << EscapeString(absl::get<1>(match.value).value) << "\""
           << std::endl;
        ss << indent << indent << indent << indent << "mask: \""
           << EscapeString(absl::get<1>(match.value).mask) << "\"" << std::endl;
        ss << indent << indent << indent << "}" << std::endl;
        break;
      default:
        throw absl::bad_variant_access();
    }
    ss << indent << indent << "}" << std::endl;
    ss << indent << "}" << std::endl;
  }
  try {
    ss << indent << "action {" << std::endl;
    ss << indent << indent << "name: \"" << EscapeString(ir.action.value().name)
       << "\"" << std::endl;
    for (const auto &param : ir.action.value().params) {
      ss << indent << indent << "param {" << std::endl;
      ss << indent << indent << indent << "name: \""
         << EscapeString(param.name) << "\"" << std::endl;
      ss << indent << indent << indent << "value: \""
         << EscapeString(param.value) << "\"" << std::endl;
      ss << indent << indent << "}" << std::endl;
    }
    ss << indent << "}" << std::endl;
  } catch (const absl::bad_optional_access& e) {
    // Not having an action is fine for a delete operation
  }
  try {
    ss << indent << "priority: " << ir.priority.value() << std::endl;
  } catch (const absl::bad_optional_access& e) {
    // Not having a priority is fine
  }
  ss << indent << "controller_metadata: \""
     << EscapeString(ir.controller_metadata) << "\"" << std::endl;
  ss << "}" << std::endl;
  return ss.str();
}

}  // namespace pdpi
