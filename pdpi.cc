#include "pdpi.h"
#include <sstream>
#include "util.h"
#include <arpa/inet.h>
#include <endian.h>

namespace pdpi {
using google::protobuf::FieldDescriptor;

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
    for (const auto match_field : table.match_fields()) {
      const auto &match_insert = tables.match_fields.insert(
          {match_field.id(), match_field});
      if (!match_insert.second) {
        throw std::invalid_argument(absl::StrCat("Duplicate match_field found ",
                                                 "with ID ", match_field.id(),
                                                 "."));
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

// Checks that pi_bytes fits into bitwdith many bits, and returns the value
// as a uint64_t.
uint64_t PiByteStringToUint(const std::string& pi_bytes, int bitwidth) {
  if (bitwidth > 64) {
    throw internal_error(absl::StrCat("Cannot convert value with "
                                             "bitwidth ", bitwidth,
                                             " to uint."));
  }
  std::string stripped_value = pi_bytes;
  RemoveLeadingZeros(&stripped_value);
  if (stripped_value.length() > 8) {
    throw std::invalid_argument(absl::StrCat("Cannot convert value longer ",
                                             "than 8 bytes to uint. ",
                                             "Length of ", stripped_value,
                                             " is ", stripped_value.length(),
                                             "."));
  }
  uint64_t nb_value; // network byte order
  char value[sizeof(nb_value)];
  int pad = sizeof(nb_value) - stripped_value.size();
  if (pad) {
    memset(value, 0, pad);
  }
  memcpy(value + pad, stripped_value.data(), stripped_value.size());
  memcpy(&nb_value, value, sizeof(nb_value));

  uint64_t pd_value = be64toh(nb_value);

  int bits_needed = 0;
  uint64_t pd = pd_value;
  while (pd > 0) {
    pd >>= 1;
    ++bits_needed;
  }
  if (bits_needed > bitwidth) {
    throw std::invalid_argument(absl::StrCat("PI value uses ", bits_needed,
                                             " bits and does not fit into ",
                                             bitwidth, " bits."));
  }
  return pd_value;
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

// Verifies the contents of the PI representation and translates to the PD
// message
void PiMatchFieldToPd(const p4::config::v1::MatchField &match_metadata,
                      const p4::v1::FieldMatch &pi_match,
                      google::protobuf::Message *pd_match_entry) {
  switch (match_metadata.match_type()) {
    case p4::config::v1::MatchField_MatchType_EXACT:
      {
        if (!pi_match.has_exact()) {
          throw std::invalid_argument(absl::StrCat("Expected exact match type ",
                                                   "in PI."));
        }

        std::string fieldname = ProtoFriendlyName(match_metadata.name());
        auto *field = GetFieldDescriptorByName(fieldname, pd_match_entry);
        uint32_t bitwidth = match_metadata.bitwidth();

        const std::string &pi_value = pi_match.exact().value();
        PiFieldToPd(*field, bitwidth, pi_value, pd_match_entry);
        break;
      }
    default:
      throw std::invalid_argument(
          absl::StrCat("Unsupported match type ",
                       p4::config::v1::MatchField_MatchType_Name(
                           match_metadata.match_type()),
                       " in ", pd_match_entry->GetTypeName(), "."));
  }
}

// Translate all matches from their PI form to the PD representations
void PiMatchesToPd(const P4TableMetadata &table_metadata,
                   const p4::v1::TableEntry &pi,
                   google::protobuf::Message *pd_table_entry) {
  std::unordered_set<uint32_t> used_field_ids;
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
    p4::config::v1::MatchField match_metadata = found_match->second;
    try {
      PiMatchFieldToPd(match_metadata, pi_match, pd_match_entry);
    } catch (const std::invalid_argument& e) {
      throw std::invalid_argument(absl::StrCat("Could not convert the match ",
                                               "field with ID ",
                                               pi_match.field_id(), ": ",
                                               e.what()));
    }
  }

  int match_fields_size = table_metadata.match_fields.size();
  if (pi.match().size() != match_fields_size) {
    throw std::invalid_argument(absl::StrCat("Expected ",
                                             table_metadata.match_fields.size(),
                                             " match conditions but found ",
                                             pi.match().size(), " instead."));
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
void PiToPd(const P4InfoMetadata &metadata,
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
