#include "src/pdpi.h"

#include <arpa/inet.h>
#include <endian.h>

#include "absl/strings/str_cat.h"
#include "src/meta.h"
#include "src/util.h"

namespace pdpi {
using google::protobuf::FieldDescriptor;
using ::p4::config::v1::MatchField;

// Converts the PI value to a PD value and stores it in the PD message
void PiFieldToPd(const FieldDescriptor &field,
                 const int bitwidth,
                 const std::string &pi_value,
                 google::protobuf::Message *parent_message) {
  std::string stripped_value = Normalize(pi_value, bitwidth);
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

  std::string stripped_value = Normalize(lpm.value(), bitwidth);

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
  std::string stripped_mask = Normalize(ternary.mask(),
                                        bitwidth);
  if (stripped_mask == "\x00") {
    // Don't care. Don't populate PD
    return;
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
                                             Normalize(ternary.value(),
                                                       bitwidth));

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
  absl::flat_hash_set<uint32_t> used_field_ids;
  int mandatory_matches = 0;
  auto *pd_match_entry = GetMessageByFieldname(kFieldMatchFieldname,
                                               pd_table_entry);
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
                                                 pd_table_entry->GetTypeName(),
                                                 "."));
    try {
      if(PiMatchFieldToPd(match_metadata.match_field,
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

// Translate the action from its PI form to the PD representation
void PiActionToPd(const P4InfoMetadata &metadata,
                  const p4::v1::TableEntry &pi,
                  google::protobuf::Message *pd_table_entry) {
  auto *pd_action_entry = GetMessageByFieldname(kActionFieldname,
                                                pd_table_entry);
  const auto &table = FindElement(metadata.tables,
                                  pi.table_id(),
                                  absl::StrCat("Table ID ", pi.table_id(),
                                               " missing in metadata."));
  if (!pi.has_action()) {
    throw std::invalid_argument(absl::StrCat("Action field missing in ",
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
        auto *pd_oneof_action = GetMessageByFieldname(
            ActionFieldname(action_metadata.preamble.alias()), pd_action_entry);
        absl::flat_hash_set<uint32_t> used_params;
        for (const auto &param : pi_action.params()) {
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
          auto *field = GetFieldDescriptorByName(
              ProtoFriendlyName(param_metadata.param.name()), pd_oneof_action);

          PiFieldToPd(*field, param_metadata.param.bitwidth(),
                      param.value(), pd_oneof_action);
        }
        break;
      }
    default:
      throw std::invalid_argument(absl::StrCat("Unsupported action type: ",
                                               pi.action().type_case()));
  }
}

// Translate a TableEntry message from PI to PD
void PiTableEntryToPd(const P4InfoMetadata &metadata,
                      const p4::v1::TableEntry &pi,
                      google::protobuf::Message *pd) {
  const auto &table = FindElement(metadata.tables, pi.table_id(),
                                  absl::StrCat("Table ID ", pi.table_id(),
                                               " missing in metadata."));
  const std::string fieldname = TableEntryFieldname(table.preamble.alias());
  auto *table_entry = GetMessageByFieldname(fieldname, pd);

  PiMatchesToPd(table, pi, table_entry);
  PiActionToPd(metadata, pi, table_entry);
}
}  // namespace pdpi
