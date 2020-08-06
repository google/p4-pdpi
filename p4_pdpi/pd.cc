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

#include "p4_pdpi/pd.h"

#include "gutil/collections.h"
#include "gutil/proto.h"
#include "p4_pdpi/utils/ir.h"

namespace pdpi {

using ::google::protobuf::FieldDescriptor;
using ::gutil::InvalidArgumentErrorBuilder;
using ::gutil::UnimplementedErrorBuilder;
using ::p4::config::v1::MatchField;

// Translate all matches from their IR form to the PD representations
void IrToPd(const IrTableEntry &ir, google::protobuf::Message *pd) {
  // Commented out till new PD definition is available
  /*
  auto *pd_table_entry =
      GetMessageByFieldname(TableEntryFieldname(ir.table_name), pd);

  // Copy over the FieldMatches
  auto *pd_match_entry =
      GetMessageByFieldname(kFieldMatchFieldname, pd_table_entry);
  for (const auto ir_match : ir.matches) {
    std::string fieldname = ProtoFriendlyName(ir_match.name);
    auto *field = GetFieldDescriptorByName(fieldname, pd_match_entry);
    absl::visit(overloaded{
                    [&pd_match_entry, &field](const std::string &s) {
                      pd_match_entry->GetReflection()->SetString(pd_match_entry,
                                                                 field, s);
                    },
                    [&pd_match_entry, &field](const IrTernaryMatch &ternary) {
                      auto *value_field = GetFieldDescriptorByName(
                          kTernaryValueFieldname, pd_match_entry);
                      pd_table_entry->GetReflection()->SetString(
                          pd_match_entry, value_field, ternary.value);

                      auto *mask_field = GetFieldDescriptorByName(
                          kTernaryMaskFieldname, pd_match_entry);
                      pd_table_entry->GetReflection()->SetString(
                          pd_match_entry, mask_field, ternary.mask);
                    },
                },
                ir_match.value);
  }

  // Copy over the Action if any
  if (ir.action.has_value()) {
    auto *pd_action_entry =
        GetMessageByFieldname(kActionFieldname, pd_table_entry);
    auto *pd_oneof_action = GetMessageByFieldname(
        ActionFieldname(ir.action.value().name), pd_action_entry);
    for (const auto &param : ir.action.value().params) {
      auto *field = GetFieldDescriptorByName(ProtoFriendlyName(param.name),
                                             pd_oneof_action);
      pd_oneof_action->GetReflection()->SetString(pd_oneof_action, field,
                                                  param.value);
    }
  }
  */
}

namespace {

gutil::StatusOr<const google::protobuf::FieldDescriptor *> GetFieldDescriptor(
    const google::protobuf::Message &parent_message,
    const std::string &fieldname) {
  auto *parent_descriptor = parent_message.GetDescriptor();
  auto *field_descriptor = parent_descriptor->FindFieldByName(fieldname);
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message.GetTypeName() << ".";
  }
  return field_descriptor;
}

gutil::StatusOr<google::protobuf::Message *> GetMutableMessage(
    google::protobuf::Message *parent_message, const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*parent_message, fieldname));
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message->GetTypeName() << ". "
           << kPdProtoAndP4InfoOutOfSync;
  }

  return parent_message->GetReflection()->MutableMessage(parent_message,
                                                         field_descriptor);
}

gutil::StatusOr<const google::protobuf::Message *> GetMessageField(
    const google::protobuf::Message &parent_message,
    const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(parent_message, fieldname));
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message.GetTypeName() << ". "
           << kPdProtoAndP4InfoOutOfSync;
  }

  return &parent_message.GetReflection()->GetMessage(parent_message,
                                                     field_descriptor);
}

absl::Status ValidateFieldDescriptorType(const FieldDescriptor *descriptor,
                                         FieldDescriptor::Type expected_type) {
  if (expected_type != descriptor->type()) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected field \"" << descriptor->name() << "\" to be of type \""
           << FieldDescriptor::TypeName(expected_type) << "\", but got \""
           << FieldDescriptor::TypeName(descriptor->type()) << "\" instead.";
  }
  return absl::OkStatus();
}

gutil::StatusOr<bool> GetBoolField(const google::protobuf::Message &message,
                                   const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BOOL));
  return message.GetReflection()->GetBool(message, field_descriptor);
}

gutil::StatusOr<int32_t> GetInt32Field(const google::protobuf::Message &message,
                                       const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_INT32));
  return message.GetReflection()->GetInt32(message, field_descriptor);
}

gutil::StatusOr<int64_t> GetInt64Field(const google::protobuf::Message &message,
                                       const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_INT64));
  return message.GetReflection()->GetInt64(message, field_descriptor);
}

gutil::StatusOr<uint64_t> GetUint64Field(
    const google::protobuf::Message &message, const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_UINT64));
  return message.GetReflection()->GetUInt64(message, field_descriptor);
}

gutil::StatusOr<std::string> GetStringField(
    const google::protobuf::Message &message, const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_STRING));
  return message.GetReflection()->GetString(message, field_descriptor);
}

absl::Status SetBoolField(google::protobuf::Message *message,
                          const std::string &fieldname, bool value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BOOL));
  message->GetReflection()->SetBool(message, field_descriptor, value);
  return absl::OkStatus();
}

absl::Status SetInt32Field(google::protobuf::Message *message,
                           const std::string &fieldname, int32_t value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_INT32));
  message->GetReflection()->SetInt32(message, field_descriptor, value);
  return absl::OkStatus();
}

absl::Status SetInt64Field(google::protobuf::Message *message,
                           const std::string &fieldname, int64_t value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_INT64));
  message->GetReflection()->SetInt64(message, field_descriptor, value);
  return absl::OkStatus();
}

absl::Status SetUint64Field(google::protobuf::Message *message,
                            const std::string &fieldname, uint64_t value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_UINT64));
  message->GetReflection()->SetUInt64(message, field_descriptor, value);
  return absl::OkStatus();
}

absl::Status SetStringField(google::protobuf::Message *message,
                            const std::string &fieldname, std::string value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_STRING));
  message->GetReflection()->SetString(message, field_descriptor, value);
  return absl::OkStatus();
}

std::vector<std::string> GetAllFieldNames(
    const google::protobuf::Message &message) {
  std::vector<const FieldDescriptor *> fields;
  message.GetReflection()->ListFields(message, &fields);
  std::vector<std::string> field_names;
  for (const auto *field : fields) {
    field_names.push_back(field->name());
  }
  return field_names;
}
}  // namespace

absl::Status PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                              const p4::v1::TableEntry &pi,
                              google::protobuf::Message *pd) {
  ASSIGN_OR_RETURN(const auto &info, CreateIrP4Info(p4_info));
  ASSIGN_OR_RETURN(const auto &ir_entry, PiTableEntryToIr(info, pi));
  IrToPd(ir_entry, pd);

  return absl::OkStatus();
}

absl::Status IrReadRequestToPd(const IrP4Info &info, const IrReadRequest &ir,
                               google::protobuf::Message *pd) {
  if (ir.device_id() == 0) {
    return UnimplementedErrorBuilder() << "Device ID missing.";
  }
  RETURN_IF_ERROR(SetUint64Field(pd, "device_id", ir.device_id()));
  if (ir.read_counter_data()) {
    RETURN_IF_ERROR(
        SetBoolField(pd, "read_counter_data", ir.read_counter_data()));
  }
  if (ir.read_meter_configs()) {
    RETURN_IF_ERROR(
        SetBoolField(pd, "read_meter_configs", ir.read_meter_configs()));
  }
  return absl::OkStatus();
}

gutil::StatusOr<IrReadRequest> PdReadRequestToIr(
    const IrP4Info &info, const google::protobuf::Message &read_request) {
  IrReadRequest result;
  ASSIGN_OR_RETURN(auto device_id, GetUint64Field(read_request, "device_id"));
  if (device_id == 0) {
    return InvalidArgumentErrorBuilder() << "Device ID missing.";
  }
  result.set_device_id(device_id);
  ASSIGN_OR_RETURN(auto read_counter_data,
                   GetBoolField(read_request, "read_counter_data"));
  result.set_read_counter_data(read_counter_data);
  ASSIGN_OR_RETURN(auto read_meter_configs,
                   GetBoolField(read_request, "read_meter_configs"));
  result.set_read_meter_configs(read_meter_configs);

  return result;
}

// Converts all PD matches to their IR form and stores them in the matches field
// of ir_table_entry.
absl::Status PdMatchEntryToIr(const IrTableDefinition &ir_table_info,
                              const google::protobuf::Message &pd_match,
                              IrTableEntry *ir_table_entry) {
  for (const auto &pd_match_name : GetAllFieldNames(pd_match)) {
    auto *ir_match = ir_table_entry->add_matches();
    ir_match->set_name(pd_match_name);
    ASSIGN_OR_RETURN(const auto &ir_match_info,
                     gutil::FindOrStatus(ir_table_info.match_fields_by_name(),
                                         pd_match_name),
                     _ << "P4Info for table \""
                       << ir_table_info.preamble().name()
                       << "\" does not contain match with name \""
                       << pd_match_name << "\".");
    switch (ir_match_info.match_field().match_type()) {
      case MatchField::EXACT: {
        ASSIGN_OR_RETURN(const auto &pd_value,
                         GetStringField(pd_match, pd_match_name));
        ASSIGN_OR_RETURN(
            *ir_match->mutable_exact(),
            FormattedStringToIrValue(pd_value, ir_match_info.format()));
        break;
      }
      case MatchField::LPM: {
        auto *ir_lpm = ir_match->mutable_lpm();
        ASSIGN_OR_RETURN(const auto *pd_lpm,
                         GetMessageField(pd_match, pd_match_name));

        ASSIGN_OR_RETURN(const auto &pd_value,
                         GetStringField(*pd_lpm, "value"));
        ASSIGN_OR_RETURN(
            *ir_lpm->mutable_value(),
            FormattedStringToIrValue(pd_value, ir_match_info.format()));

        ASSIGN_OR_RETURN(const auto &pd_prefix_len,
                         GetInt32Field(*pd_lpm, "prefix_length"));
        if (pd_prefix_len < 0 ||
            pd_prefix_len > ir_match_info.match_field().bitwidth()) {
          return InvalidArgumentErrorBuilder()
                 << "Prefix length (" << pd_prefix_len << ") for match field \""
                 << ir_match->name() << "\" is out of bounds.";
        }
        ir_lpm->set_prefix_length(pd_prefix_len);
        break;
      }
      case MatchField::TERNARY: {
        auto *ir_ternary = ir_match->mutable_ternary();
        ASSIGN_OR_RETURN(const auto *pd_ternary,
                         GetMessageField(pd_match, pd_match_name));

        ASSIGN_OR_RETURN(const auto &pd_value,
                         GetStringField(*pd_ternary, "value"));
        ASSIGN_OR_RETURN(
            *ir_ternary->mutable_value(),
            FormattedStringToIrValue(pd_value, ir_match_info.format()));

        ASSIGN_OR_RETURN(const auto &pd_mask,
                         GetStringField(*pd_ternary, "mask"));
        ASSIGN_OR_RETURN(
            *ir_ternary->mutable_mask(),
            FormattedStringToIrValue(pd_mask, ir_match_info.format()));
        break;
      }
      default:
        return gutil::InvalidArgumentErrorBuilder()
               << "Unsupported match type \""
               << MatchField_MatchType_Name(
                      ir_match_info.match_field().match_type())
               << "\" in \"" << pd_match_name << "\".";
    }
  }
  return absl::OkStatus();
}

// Converts a PD action invocation to its IR form and returns it.
gutil::StatusOr<IrActionInvocation> PdActionInvocationToIr(
    const IrP4Info &ir_p4info, const std::string &action_name,
    const google::protobuf::Message &pd_action) {
  ASSIGN_OR_RETURN(
      const auto &ir_action_info,
      gutil::FindOrStatus(ir_p4info.actions_by_name(), action_name),
      _ << "P4Info does not contain action with name \"" << action_name
        << "\".");
  IrActionInvocation ir_action;
  ir_action.set_name(action_name);
  for (const auto &pd_arg_name : GetAllFieldNames(pd_action)) {
    ASSIGN_OR_RETURN(
        const auto &param_info,
        gutil::FindOrStatus(ir_action_info.params_by_name(), pd_arg_name));
    ASSIGN_OR_RETURN(const auto &pd_arg,
                     GetStringField(pd_action, pd_arg_name));
    auto *ir_param = ir_action.add_params();
    ir_param->set_name(pd_arg_name);
    ASSIGN_OR_RETURN(*ir_param->mutable_value(),
                     FormattedStringToIrValue(pd_arg, param_info.format()));
  }
  return ir_action;
}

// Converts a PD action set to its IR form and stores it in the
// ir_table_entry.
gutil::StatusOr<IrActionSetInvocation> PdActionSetToIr(
    const IrP4Info &ir_p4info, const google::protobuf::Message &pd_action_set) {
  IrActionSetInvocation ir_action_set_invocation;
  for (const auto &pd_field_name : GetAllFieldNames(pd_action_set)) {
    if (pd_field_name == "weight") {
      ASSIGN_OR_RETURN(const auto &pd_weight,
                       GetInt32Field(pd_action_set, "weight"));
      ir_action_set_invocation.set_weight(pd_weight);
    } else {
      ASSIGN_OR_RETURN(const auto *pd_action,
                       GetMessageField(pd_action_set, pd_field_name));
      ASSIGN_OR_RETURN(
          *ir_action_set_invocation.mutable_action(),
          PdActionInvocationToIr(ir_p4info, pd_field_name, *pd_action));
    }
  }
  return ir_action_set_invocation;
}

gutil::StatusOr<IrTableEntry> PdTableEntryToIr(
    const IrP4Info &ir_p4info, const google::protobuf::Message &pd) {
  IrTableEntry ir;
  ASSIGN_OR_RETURN(const auto &pd_table_name,
                   gutil::GetOneOfFieldName(pd, "entry"));
  ASSIGN_OR_RETURN(
      const auto &ir_table_info,
      gutil::FindOrStatus(ir_p4info.tables_by_name(), pd_table_name),
      _ << "Table \"" << pd_table_name << "\" does not exist in P4Info."
        << kPdProtoAndP4InfoOutOfSync);
  ir.set_table_name(pd_table_name);

  ASSIGN_OR_RETURN(const auto *pd_table, GetMessageField(pd, pd_table_name));

  ASSIGN_OR_RETURN(const auto *pd_match, GetMessageField(*pd_table, "match"));
  RETURN_IF_ERROR(PdMatchEntryToIr(ir_table_info, *pd_match, &ir));

  if (ir_table_info.uses_oneshot()) {
    ASSIGN_OR_RETURN(const auto *pd_action_set,
                     GetFieldDescriptor(*pd_table, "actions"));
    auto *action_set = ir.mutable_action_set();
    for (auto i = 0;
         i < pd_table->GetReflection()->FieldSize(*pd_table, pd_action_set);
         ++i) {
      ASSIGN_OR_RETURN(
          *action_set->add_actions(),
          PdActionSetToIr(ir_p4info,
                          pd_table->GetReflection()->GetRepeatedMessage(
                              *pd_table, pd_action_set, i)));
    }
  } else {
    ASSIGN_OR_RETURN(const auto *pd_action,
                     GetMessageField(*pd_table, "action"));
    for (const auto &action_name : GetAllFieldNames(*pd_action)) {
      ASSIGN_OR_RETURN(const auto *pd_action_invocation,
                       GetMessageField(*pd_action, action_name));
      ASSIGN_OR_RETURN(*ir.mutable_action(),
                       PdActionInvocationToIr(ir_p4info, action_name,
                                              *pd_action_invocation));
    }
  }

  if (ir_table_info.has_meter()) {
    ASSIGN_OR_RETURN(const auto *config,
                     GetMessageField(*pd_table, "meter_config"));
    int64_t value;
    int64_t burst_value;
    switch (ir_table_info.meter().unit()) {
      case p4::config::v1::MeterSpec_Unit_BYTES: {
        ASSIGN_OR_RETURN(value, GetInt64Field(*config, "bytes_per_second"));
        ASSIGN_OR_RETURN(burst_value, GetInt64Field(*config, "burst_bytes"));
        break;
      }
      case p4::config::v1::MeterSpec_Unit_PACKETS: {
        ASSIGN_OR_RETURN(value, GetInt64Field(*config, "packets_per_second"));
        ASSIGN_OR_RETURN(burst_value, GetInt64Field(*config, "burst_packets"));
        break;
      }
      default:
        return InvalidArgumentErrorBuilder()
               << "Invalid meter unit: " << ir_table_info.meter().unit();
    }
    auto ir_meter_config = ir.mutable_meter_config();
    ir_meter_config->set_cir(value);
    ir_meter_config->set_pir(value);
    ir_meter_config->set_cburst(burst_value);
    ir_meter_config->set_pburst(burst_value);
  }

  if (ir_table_info.has_counter()) {
    switch (ir_table_info.counter().unit()) {
      case p4::config::v1::MeterSpec_Unit_BYTES: {
        ASSIGN_OR_RETURN(const auto &pd_byte_counter,
                         GetInt64Field(*pd_table, "byte_counter"));
        ir.mutable_counter_data()->set_byte_count(pd_byte_counter);
        break;
      }
      case p4::config::v1::MeterSpec_Unit_PACKETS: {
        ASSIGN_OR_RETURN(const auto &pd_packet_counter,
                         GetInt64Field(*pd_table, "packet_counter"));
        ir.mutable_counter_data()->set_packet_count(pd_packet_counter);
        break;
      }
      default:
        return InvalidArgumentErrorBuilder()
               << "Invalid counter unit: " << ir_table_info.meter().unit();
    }
  }

  return ir;
}

absl::Status IrTableEntryToPd(const IrP4Info &ir_p4info, const IrTableEntry &ir,
                              const google::protobuf::Message *pd) {
  return absl::OkStatus();
}
}  // namespace pdpi
