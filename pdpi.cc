#include "pdpi.h"

#include <sstream>
#include <arpa/inet.h>
#include <endian.h>

#include "util.h"

namespace pdpi {
using google::protobuf::FieldDescriptor;
using ::p4::config::v1::MatchField;

// Copies the metadata into a stream, converts it to a string and returns it
std::string MetadataToString(const P4InfoMetadata &metadata) {
  std::stringstream ss;
  ss << "Table ID to Table Metadata" << std::endl;
  for (const auto &entry : metadata.tables) {
    ss << "Table ID: " << entry.first << std::endl
       << "\tPreamble:" << std::endl;
    p4::config::v1::Preamble preamble = entry.second.preamble;
    ss << "\t\tName: " << preamble.name() << std::endl
       << "\t\tAlias: " << preamble.alias() << std::endl;
    for (const auto &annotation : preamble.annotations()) {
      ss << "\t\tAnnotation: " << annotation << std::endl;
    }
    for (const auto &match_field : entry.second.match_fields) {
      ss << "\tMatch Field ID: " << match_field.first << std::endl
         << "\t\tName: " << match_field.second.name() << std::endl;
      for (const auto &annotation : match_field.second.annotations()) {
        ss << "\t\tAnnotation: " << annotation << std::endl;
      }
      ss << "\t\tBitwidth: " << match_field.second.bitwidth() << std::endl
         << "\t\tMatch Type: " << match_field.second.match_type() << std::endl;
    }
    ss << "\tNumber of Mandatory Match Fields: "
       << entry.second.num_mandatory_match_fields
       << std::endl;
    ss << "\tValid Actions: ";
    for (const auto &action: entry.second.valid_actions) {
      ss << action << ",";
    }
    ss << std::endl;
    ss << "\tSize: " << entry.second.size << std::endl;
  }

  ss << "Action ID to Action" << std::endl;
  for (const auto &entry : metadata.actions) {
    ss << "Action ID: " << entry.first << std::endl
       << "\tPreamble:" << std::endl;
    p4::config::v1::Preamble preamble = entry.second.preamble;
    ss << "\t\tName: " << preamble.name() << std::endl
       << "\t\tAlias: " << preamble.alias() << std::endl;
    for (const auto &param : entry.second.params) {
      ss << "\tParam ID: " << param.first << std::endl
         << "\t\tName: " << param.second.name() << std::endl
         << "\t\tBitwidth: " << param.second.bitwidth() << std::endl;
      for (const auto &annotation : param.second.annotations()) {
        ss << "\t\tAnnotation: " << annotation << std::endl;
      }
    }
  }

  return ss.str();
}

// Copies the data from the P4Info into maps for easy access during translation
P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info) {
  P4InfoMetadata metadata;
  // Saves all the actions for easy access.
  for (const auto action : p4_info.actions()) {
    P4ActionMetadata action_metadata;
    const auto preamble = action.preamble();
    action_metadata.preamble = preamble;
    for (const auto param : action.params()) {
      const auto &param_insert = action_metadata.params.insert(
          {param.id(), param});
      if (!param_insert.second) {
        throw std::invalid_argument(absl::StrCat("Duplicate param found ",
                                                 "with ID ",param.id(),
                                                 " for action ",
                                                 action.preamble().id(), "."));
      }
    }
    const auto &action_insert = metadata.actions.insert(
        {action.preamble().id(), action_metadata});
    if (!action_insert.second) {
      throw std::invalid_argument(absl::StrCat("Duplicate action found with ",
                                               "ID ", action.preamble().id(),
                                               "."));
    }
  }

  // Saves the table definitions into maps to have easy access to various parts
  // of it as needed.
  for (const auto &table : p4_info.tables()) {
    // Not using reference since preamble will be stored in map and P4Info may
    // or may not be available at that point.
    // Will make a copy on heap in future since same memory can be pointed to in
    // maps used for PD to PI conversions.
    // Same case for match_field below.
    struct P4TableMetadata tables;
    const auto preamble = table.preamble();
    tables.preamble = preamble;
    tables.num_mandatory_match_fields = 0;
    for (const auto match_field : table.match_fields()) {
      const auto &match_insert = tables.match_fields.insert(
          {match_field.id(), match_field});
      if (!match_insert.second) {
        throw std::invalid_argument(absl::StrCat("Duplicate match_field found ",
                                                 "with ID ", match_field.id(),
                                                 "."));
      }
      if (match_field.match_type() == MatchField::EXACT) {
        ++tables.num_mandatory_match_fields;
      }
    }
    for (const auto &action_ref : table.action_refs()) {
      tables.valid_actions.insert(action_ref.id());
    }
    tables.size = table.size();
    const auto &table_insert = metadata.tables.insert(
        {table.preamble().id(), tables});
    if (!table_insert.second) {
      throw std::invalid_argument(absl::StrCat("Duplicate table found with ",
                                               "ID ", table.preamble().id(),
                                               "."));
    }
  }

  return metadata;
}

// Converts the PI value to a PD value and stores it in the PD message
void PiFieldToPd(const FieldDescriptor &field,
                 const int bitwidth,
                 const std::string &pi_value,
                 google::protobuf::Message *parent_message) {
  std::string stripped_value = pi_value;
  RemoveLeadingZeros(&stripped_value);
  if (bitwidth > 64) {
    if (field.type() != FieldDescriptor::TYPE_BYTES) {
      throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                               " should be stored in a field ",
                                               " with type ",
                                               FieldDescriptor::TypeName(
                                                   FieldDescriptor::TYPE_BYTES),
                                               ", not ",
                                               FieldDescriptor::TypeName(
                                                   field.type()), ". "
                                               , kPdProtoAndP4InfoOutOfSync));
    }
    parent_message->GetReflection()->SetString(parent_message,
                                               &field,
                                               stripped_value);
    return;
  }

  uint64_t pd_value = PiByteStringToUint(stripped_value, bitwidth);
  if (bitwidth == 1) {
    if (field.type() != FieldDescriptor::TYPE_BOOL) {
      throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                               " should be stored in a field ",
                                               " with type ",
                                               FieldDescriptor::TypeName(
                                                   FieldDescriptor::TYPE_BOOL),
                                               ", not ",
                                               FieldDescriptor::TypeName(
                                                   field.type()), ". "
                                               , kPdProtoAndP4InfoOutOfSync));
    }
    parent_message->GetReflection()->SetBool(parent_message,
                                             &field,
                                             pd_value);
  } else if (bitwidth <= 32) {
    if (field.type() != FieldDescriptor::TYPE_UINT32) {
      throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                               " should be stored in a field ",
                                               " with type ",
                                               FieldDescriptor::TypeName(
                                                  FieldDescriptor::TYPE_UINT32),
                                               ", not ",
                                               FieldDescriptor::TypeName(
                                                   field.type()), ". "
                                               , kPdProtoAndP4InfoOutOfSync));
    }
    parent_message->GetReflection()->SetUInt32(parent_message,
                                               &field,
                                               pd_value);
  } else {
    if (field.type() != FieldDescriptor::TYPE_UINT64) {
      throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                               " should be stored in a field ",
                                               " with type ",
                                               FieldDescriptor::TypeName(
                                                  FieldDescriptor::TYPE_UINT64),
                                               ", not ",
                                               FieldDescriptor::TypeName(
                                                   field.type()), ". "
                                               , kPdProtoAndP4InfoOutOfSync));
    }
    parent_message->GetReflection()->SetUInt64(parent_message,
                                               &field,
                                               pd_value);
  }
}

// Converts the PI LPM values to PD and stores it in the PD message
void PiLpmToPd(const int bitwidth,
               const p4::v1::FieldMatch_LPM &lpm,
               google::protobuf::Message *parent_message) {
  int prefix_len = lpm.prefix_len();
  if (prefix_len == 0) {
    // Don't care. Don't populate PD
    return;
  }

  if (prefix_len > bitwidth) {
    throw std::invalid_argument(absl::StrCat("Prefix length ", prefix_len,
                                             " is greater than bitwidth ",
                                             bitwidth, " in LPM in ",
                                             parent_message->GetTypeName()));
  }

  std::string stripped_value = lpm.value();
  RemoveLeadingZeros(&stripped_value);

  int stripped_length_in_bits = GetBitwidthOfPiByteString(stripped_value);
  if (stripped_length_in_bits > bitwidth) {
    throw std::invalid_argument(absl::StrCat("Length of value ",
                                             stripped_length_in_bits,
                                             " is greater than bitwidth ",
                                             bitwidth, " in LPM in ",
                                             parent_message->GetTypeName()));
  }

  auto *value_field = GetFieldDescriptorByName(kLpmValueFieldname,
                                               parent_message);
  if (value_field->type() != FieldDescriptor::TYPE_BYTES) {
    throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                             " should be stored in a field ",
                                             " with type ",
                                             FieldDescriptor::TypeName(
                                                 FieldDescriptor::TYPE_BYTES),
                                             ", not ",
                                             FieldDescriptor::TypeName(
                                                 value_field->type()), ". "
                                             , kPdProtoAndP4InfoOutOfSync));
  }
  parent_message->GetReflection()->SetString(parent_message,
                                             value_field,
                                             stripped_value);

  auto *prefix_field = GetFieldDescriptorByName(kLpmPrefixLenFieldname,
                                                parent_message);
  if (prefix_field->type() != FieldDescriptor::TYPE_INT32) {
    throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                             " should be stored in a field ",
                                             " with type ",
                                             FieldDescriptor::TypeName(
                                                 FieldDescriptor::TYPE_INT32),
                                             ", not ",
                                             FieldDescriptor::TypeName(
                                                 prefix_field->type()), ". "
                                             , kPdProtoAndP4InfoOutOfSync));
  }
  parent_message->GetReflection()->SetInt32(parent_message,
                                            prefix_field,
                                            prefix_len);
}

// Converts the PI Ternary values to PD and stores it in the PD message
void PiTernaryToPd(const int bitwidth,
                   const p4::v1::FieldMatch_Ternary &ternary,
                   google::protobuf::Message *parent_message) {
  std::string stripped_mask = ternary.mask();
  RemoveLeadingZeros(&stripped_mask);

  if (stripped_mask == "\x00") {
    // Don't care. Don't populate PD
    return;
  }

  int stripped_mask_length_in_bits = GetBitwidthOfPiByteString(stripped_mask);
  if (stripped_mask_length_in_bits > bitwidth) {
    throw std::invalid_argument(absl::StrCat("Length of mask ",
                                             stripped_mask_length_in_bits,
                                             " is greater than bitwidth ",
                                             bitwidth, " in ternary match in ",
                                             parent_message->GetTypeName()));
  }

  std::string stripped_value = ternary.value();
  RemoveLeadingZeros(&stripped_value);

  int stripped_value_length_in_bits = GetBitwidthOfPiByteString(stripped_value);
  if (stripped_value_length_in_bits > bitwidth) {
    throw std::invalid_argument(absl::StrCat("Length of value ",
                                             stripped_value_length_in_bits,
                                             " is greater than bitwidth ",
                                             bitwidth, " in ternary match in ",
                                             parent_message->GetTypeName()));
  }

  auto *value_field = GetFieldDescriptorByName(kTernaryValueFieldname,
                                               parent_message);
  if (value_field->type() != FieldDescriptor::TYPE_BYTES) {
    throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                             " should be stored in a field ",
                                             " with type ",
                                             FieldDescriptor::TypeName(
                                                 FieldDescriptor::TYPE_BYTES),
                                             ", not ",
                                             FieldDescriptor::TypeName(
                                                 value_field->type()), ". "
                                             , kPdProtoAndP4InfoOutOfSync));
  }
  parent_message->GetReflection()->SetString(parent_message,
                                             value_field,
                                             stripped_value);

  auto *mask_field = GetFieldDescriptorByName(kTernaryMaskFieldname,
                                              parent_message);
  if (mask_field->type() != FieldDescriptor::TYPE_BYTES) {
    throw std::invalid_argument(absl::StrCat("Field with bitwidth ", bitwidth,
                                             " should be stored in a field ",
                                             " with type ",
                                             FieldDescriptor::TypeName(
                                                 FieldDescriptor::TYPE_BYTES),
                                             ", not ",
                                             FieldDescriptor::TypeName(
                                                 mask_field->type()), ". "
                                             , kPdProtoAndP4InfoOutOfSync));
  }
  parent_message->GetReflection()->SetString(parent_message,
                                             mask_field,
                                             stripped_mask);
}

// Verifies the contents of the PI representation and translates to the PD
// message. Returns if the match was exact to let the caller know if the match
// is mandatory
bool PiMatchFieldToPd(const MatchField &match_metadata,
                      const p4::v1::FieldMatch &pi_match,
                      google::protobuf::Message *pd_match_entry) {
  std::string fieldname = ProtoFriendlyName(match_metadata.name());
  uint32_t bitwidth = match_metadata.bitwidth();

  switch (match_metadata.match_type()) {
    case MatchField::EXACT:
      {
        if (!pi_match.has_exact()) {
          throw std::invalid_argument(absl::StrCat("Expected exact match type ",
                                                   "in PI."));
        }

        auto *field = GetFieldDescriptorByName(fieldname, pd_match_entry);
        const std::string &pi_value = pi_match.exact().value();
        PiFieldToPd(*field, bitwidth, pi_value, pd_match_entry);
        break;
      }
    case MatchField::LPM:
      {
        if (!pi_match.has_lpm()) {
          throw std::invalid_argument(absl::StrCat("Expected LPM match type ",
                                                   "in PI."));
        }

        auto *field = GetMessageByFieldname(fieldname, pd_match_entry);
        PiLpmToPd(bitwidth, pi_match.lpm(), field);
        break;
      }
    case MatchField::TERNARY:
      {
        if (!pi_match.has_ternary()) {
          throw std::invalid_argument(absl::StrCat("Expected Ternary match ",
                                                   "type in PI."));
        }

        auto *field = GetMessageByFieldname(fieldname, pd_match_entry);
        PiTernaryToPd(bitwidth, pi_match.ternary(), field);
        break;
      }
    default:
      throw std::invalid_argument(
          absl::StrCat("Unsupported match type ",
                       MatchField_MatchType_Name(
                           match_metadata.match_type()),
                       " in ", pd_match_entry->GetTypeName(), "."));
  }

  return match_metadata.match_type() == MatchField::EXACT;
}

// Translate all matches from their PI form to the PD representations
void PiMatchesToPd(const P4TableMetadata &table_metadata,
                   const p4::v1::TableEntry &pi,
                   google::protobuf::Message *pd_table_entry) {
  std::unordered_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  auto *pd_match_entry = GetMessageByFieldname("match", pd_table_entry);
  for (const auto pi_match : pi.match()) {
    const auto &id_insert = used_field_ids.insert(pi_match.field_id());
    if (!id_insert.second) {
      throw std::invalid_argument(absl::StrCat("Duplicate match field found ",
                                               "with ID ",
                                               pi_match.field_id(), "."));
    }
    const auto found_match = table_metadata.match_fields.find(
        pi_match.field_id());
    if (found_match == table_metadata.match_fields.end()) {
      throw std::invalid_argument(absl::StrCat("Match Field ",
                                               pi_match.field_id(),
                                               " not found in table ",
                                               pd_table_entry->GetTypeName(),
                                               "."));
    }
    MatchField match_metadata = found_match->second;
    try {
      if(PiMatchFieldToPd(match_metadata,
                          pi_match,
                          pd_match_entry)) {
        ++mandatory_matches;
      }
    } catch (const std::invalid_argument& e) {
      throw std::invalid_argument(absl::StrCat("Could not convert the match ",
                                               "field with ID ",
                                               pi_match.field_id(), ": ",
                                               e.what()));
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

void PiActionToPd(const P4InfoMetadata &metadata,
                   const p4::v1::TableEntry &pi,
                   google::protobuf::Message *pd_table_entry) {
  auto *pd_action_entry = GetMessageByFieldname("action", pd_table_entry);
  const auto found_table = metadata.tables.find(pi.table_id());
  if (found_table == metadata.tables.end()) {
    throw std::invalid_argument(absl::StrCat("Table ID ", pi.table_id(),
                                             " not found in metadata."));
  }
  P4TableMetadata table = found_table->second;
  if (!pi.has_action()) {
    throw std::invalid_argument(absl::StrCat("Action field not found in ",
                                             "TableEntry with ID ",
                                             pi.table_id()));
  }
  if (pi.action().has_action()) {
    const auto pi_action = pi.action().action();
    uint32_t action_id = pi_action.action_id();

    const auto found_action = metadata.actions.find(action_id);
    if (found_action == metadata.actions.end()) {
      throw std::invalid_argument(absl::StrCat("Action ID ", action_id,
                                               " not found in metadata."));
    }

    if (table.valid_actions.find(action_id) == table.valid_actions.end()) {
      throw std::invalid_argument(absl::StrCat("Action ID ", action_id,
                                               " is not a valid action ",
                                               " for table with table ID",
                                               pi.table_id(), "."));
    }

    P4ActionMetadata action_metadata = found_action->second;
    int action_params_size = action_metadata.params.size();
    if (action_params_size != pi_action.params().size()) {
      throw std::invalid_argument(absl::StrCat("Expected ",
                                               action_params_size,
                                               " parameters, but got ",
                                               pi_action.params().size(),
                                               " instead in action with ID ",
                                               action_id, "."));
    }
    auto *pd_oneof_action = GetMessageByFieldname(
        ActionFieldname(action_metadata.preamble.alias()), pd_action_entry);
    for (const auto &param : pi_action.params()) {
      std::unordered_set<uint32_t> used_params;
      const auto &id_insert = used_params.insert(param.param_id());
      if (!id_insert.second) {
        throw std::invalid_argument(absl::StrCat("Duplicate param field found ",
                                                 "with ID ",
                                                 param.param_id(), "."));
      }
      const auto found_param = action_metadata.params.find(param.param_id());
      if (found_param == action_metadata.params.end()) {
        throw std::invalid_argument(absl::StrCat("Unable to find param ID ",
                                                 param.param_id(),
                                                 " in action with ID ",
                                                 action_id));
      }
      const auto &param_metadata = found_param->second;
      auto *field = GetFieldDescriptorByName(
          ProtoFriendlyName(param_metadata.name()), pd_oneof_action);

      PiFieldToPd(*field, param_metadata.bitwidth(),
                  param.value(), pd_oneof_action);
    }
  }
}

// Translate a TableEntry message from PI to PD
void PiTableEntryToPd(const P4InfoMetadata &metadata,
                      const p4::v1::TableEntry &pi,
                      google::protobuf::Message *pd) {
  const auto found_table = metadata.tables.find(pi.table_id());
  if (found_table == metadata.tables.end()) {
    throw std::invalid_argument(absl::StrCat("Table ID ", pi.table_id(),
                                             " not found in metadata."));
  }
  P4TableMetadata table = found_table->second;
  const std::string fieldname = TableEntryFieldname(table.preamble.alias());
  auto *table_entry = GetMessageByFieldname(fieldname, pd);

  PiMatchesToPd(table, pi, table_entry);
  PiActionToPd(metadata, pi, table_entry);
}

}  // namespace pdpi
