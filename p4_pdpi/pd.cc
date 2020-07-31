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
#include "p4_pdpi/ir.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/pd.h"

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

gutil::StatusOr<uint64_t> GetUint64Field(
    const google::protobuf::Message &message, const std::string &fieldname) {
  // TODO(heule): remove this cast once atmanm@'s CL goes in
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptorByName(
                       fieldname, (google::protobuf::Message *)&message));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_UINT64));
  return message.GetReflection()->GetUInt64(message, field_descriptor);
}

gutil::StatusOr<bool> GetBoolField(const google::protobuf::Message &message,
                                   const std::string &fieldname) {
  // TODO(heule): remove this cast once atmanm@'s CL goes in
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptorByName(
                       fieldname, (google::protobuf::Message *)&message));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BOOL));
  return message.GetReflection()->GetBool(message, field_descriptor);
}

absl::Status SetUint64Field(google::protobuf::Message *message,
                            const std::string &fieldname, uint64_t value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptorByName(fieldname, message));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_UINT64));
  message->GetReflection()->SetUInt64(message, field_descriptor, value);
  return absl::OkStatus();
}

absl::Status SetBoolField(google::protobuf::Message *message,
                          const std::string &fieldname, bool value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptorByName(fieldname, message));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BOOL));
  message->GetReflection()->SetBool(message, field_descriptor, value);
  return absl::OkStatus();
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

}  // namespace pdpi
