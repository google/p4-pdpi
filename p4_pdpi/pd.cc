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

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map.h"
#include "google/protobuf/message.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "gutil/collections.h"
#include "gutil/proto.h"
#include "gutil/status.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/internal/ordered_protobuf_map.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/ir.h"
#include "p4_pdpi/utils/pd.h"

namespace pdpi {

using ::google::protobuf::FieldDescriptor;
using ::gutil::InvalidArgumentErrorBuilder;
using ::gutil::UnimplementedErrorBuilder;
using ::p4::config::v1::MatchField;

namespace {

constexpr char kPdProtoAndP4InfoOutOfSync[] =
    "The PD proto and P4Info file are out of sync.";

absl::StatusOr<const google::protobuf::FieldDescriptor *> GetFieldDescriptor(
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

absl::StatusOr<google::protobuf::Message *> GetMutableMessage(
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

absl::StatusOr<const google::protobuf::Message *> GetMessageField(
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

absl::StatusOr<const google::protobuf::Message *> GetRepeatedMessage(
    const google::protobuf::Message &parent_message,
    const std::string &fieldname, int index) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(parent_message, fieldname));
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message.GetTypeName() << ". "
           << kPdProtoAndP4InfoOutOfSync;
  }
  int repeated_field_length = parent_message.GetReflection()->FieldSize(
      parent_message, field_descriptor);
  if (parent_message.GetReflection()->FieldSize(parent_message,
                                                field_descriptor) < index) {
    return gutil::OutOfRangeErrorBuilder()
           << "Index out of repeated field's bound. field's length: "
           << repeated_field_length << "index: " << index;
  }
  return &parent_message.GetReflection()->GetRepeatedMessage(
      parent_message, field_descriptor, index);
}

absl::StatusOr<google::protobuf::Message *> AddRepeatedMutableMessage(
    google::protobuf::Message *parent_message, const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*parent_message, fieldname));
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message->GetTypeName() << ". "
           << kPdProtoAndP4InfoOutOfSync;
  }
  return parent_message->GetReflection()->AddMessage(parent_message,
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

absl::StatusOr<bool> GetBoolField(const google::protobuf::Message &message,
                                  const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BOOL));
  return message.GetReflection()->GetBool(message, field_descriptor);
}

absl::StatusOr<int32_t> GetInt32Field(const google::protobuf::Message &message,
                                      const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_INT32));
  return message.GetReflection()->GetInt32(message, field_descriptor);
}

absl::StatusOr<int64_t> GetInt64Field(const google::protobuf::Message &message,
                                      const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_INT64));
  return message.GetReflection()->GetInt64(message, field_descriptor);
}

absl::StatusOr<uint64_t> GetUint64Field(
    const google::protobuf::Message &message, const std::string &fieldname) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, fieldname));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_UINT64));
  return message.GetReflection()->GetUInt64(message, field_descriptor);
}

absl::StatusOr<std::string> GetStringField(
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
  field_names.reserve(fields.size());
  for (const auto *field : fields) {
    field_names.push_back(field->name());
  }
  return field_names;
}
}  // namespace

absl::StatusOr<int> GetEnumField(const google::protobuf::Message &message,
                                 const std::string &field_name) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(message, field_name));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_ENUM));
  int enum_value =
      message.GetReflection()->GetEnumValue(message, field_descriptor);
  if (field_descriptor->enum_type()->FindValueByNumber(enum_value) == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Enum value within " << field_name << " is : " << enum_value;
  }
  return enum_value;
}
absl::Status SetEnumField(google::protobuf::Message *message,
                          const std::string &enum_field_name, int enum_value) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*message, enum_field_name));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_ENUM));
  if (field_descriptor->enum_type()->FindValueByNumber(enum_value) == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "enum_value: " << enum_value << " is not a valid enum value ";
  }
  message->GetReflection()->SetEnumValue(message, field_descriptor, enum_value);
  return absl::OkStatus();
}

absl::Status PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                              const p4::v1::TableEntry &pi,
                              google::protobuf::Message *pd) {
  ASSIGN_OR_RETURN(const auto info, CreateIrP4Info(p4_info));
  ASSIGN_OR_RETURN(const auto ir_entry, PiTableEntryToIr(info, pi));
  RETURN_IF_ERROR(IrTableEntryToPd(info, ir_entry, pd));
  return absl::OkStatus();
}

absl::StatusOr<p4::v1::TableEntry> PdTableEntryToPi(
    const p4::config::v1::P4Info &p4_info,
    const google::protobuf::Message &pd) {
  ASSIGN_OR_RETURN(const auto info, CreateIrP4Info(p4_info));
  ASSIGN_OR_RETURN(const auto ir_entry, PdTableEntryToIr(info, pd));
  return IrTableEntryToPi(info, ir_entry);
}

absl::Status PiPacketInToPd(const IrP4Info &info,
                            const p4::v1::PacketIn &pi_packet,
                            google::protobuf::Message *pd_packet) {
  ASSIGN_OR_RETURN(const auto ir, PiPacketInToIr(info, pi_packet));
  return IrPacketInToPd(info, ir, pd_packet);
}

absl::StatusOr<p4::v1::PacketIn> PdPacketInToPi(
    const IrP4Info &info, const google::protobuf::Message &packet) {
  ASSIGN_OR_RETURN(const auto ir, PdPacketInToIr(info, packet));
  return IrPacketInToPi(info, ir);
}

absl::Status PiPacketOutToPd(const IrP4Info &info,
                             const p4::v1::PacketOut &pi_packet,
                             google::protobuf::Message *pd_packet) {
  ASSIGN_OR_RETURN(const auto ir, PiPacketOutToIr(info, pi_packet));
  return IrPacketOutToPd(info, ir, pd_packet);
}

absl::StatusOr<p4::v1::PacketOut> PdPacketOutToPi(
    const IrP4Info &info, const google::protobuf::Message &packet) {
  ASSIGN_OR_RETURN(const auto ir, PdPacketOutToIr(info, packet));
  return IrPacketOutToPi(info, ir);
}

absl::Status GrpcStatusToPd(const grpc::Status &status,
                            int number_of_updates_in_write_request,
                            google::protobuf::Message *pd) {
  ASSIGN_OR_RETURN(
      const auto ir_write_rpc_status,
      GrpcStatusToIrWriteRpcStatus(status, number_of_updates_in_write_request));
  return IrWriteRpcStatusToPd(ir_write_rpc_status, pd);
}

absl::StatusOr<grpc::Status> PdWriteRpcStatusToGrpcStatus(
    const google::protobuf::Message &pd) {
  ASSIGN_OR_RETURN(const auto ir_write_rpc_status,
                   pdpi::PdWriteRpcStatusToIr(pd));
  return IrWriteRpcStatusToGrpcStatus(ir_write_rpc_status);
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

absl::StatusOr<IrReadRequest> PdReadRequestToIr(
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

absl::Status IrReadResponseToPd(const IrP4Info &info, const IrReadResponse &ir,
                                google::protobuf::Message *read_response) {
  for (const auto &ir_table_entry : ir.table_entries()) {
    ASSIGN_OR_RETURN(const auto *table_entries_descriptor,
                     GetFieldDescriptor(*read_response, "table_entries"));
    RETURN_IF_ERROR(
        IrTableEntryToPd(info, ir_table_entry,
                         read_response->GetReflection()->AddMessage(
                             read_response, table_entries_descriptor)));
  }
  return absl::OkStatus();
}

absl::StatusOr<IrReadResponse> PdReadResponseToIr(
    const IrP4Info &info, const google::protobuf::Message &read_response) {
  IrReadResponse ir_response;
  ASSIGN_OR_RETURN(const auto table_entries_descriptor,
                   GetFieldDescriptor(read_response, "table_entries"));
  for (auto i = 0; i < read_response.GetReflection()->FieldSize(
                           read_response, table_entries_descriptor);
       ++i) {
    ASSIGN_OR_RETURN(
        *ir_response.add_table_entries(),
        PdTableEntryToIr(info,
                         read_response.GetReflection()->GetRepeatedMessage(
                             read_response, table_entries_descriptor, i)));
  }
  return ir_response;
}

absl::Status IrUpdateToPd(const IrP4Info &info, const IrUpdate &ir,
                          google::protobuf::Message *update) {
  ASSIGN_OR_RETURN(const auto *type_descriptor,
                   GetFieldDescriptor(*update, "type"));
  RETURN_IF_ERROR(
      ValidateFieldDescriptorType(type_descriptor, FieldDescriptor::TYPE_ENUM));
  update->GetReflection()->SetEnumValue(update, type_descriptor, ir.type());

  ASSIGN_OR_RETURN(auto *pd_table_entry,
                   GetMutableMessage(update, "table_entry"));
  RETURN_IF_ERROR(IrTableEntryToPd(info, ir.table_entry(), pd_table_entry));
  return absl::OkStatus();
}

absl::StatusOr<IrUpdate> PdUpdateToIr(const IrP4Info &info,
                                      const google::protobuf::Message &update) {
  IrUpdate ir_update;
  ASSIGN_OR_RETURN(const auto *type_descriptor,
                   GetFieldDescriptor(update, "type"));
  const auto &type_value =
      update.GetReflection()->GetEnumValue(update, type_descriptor);

  if (!p4::v1::Update_Type_IsValid(type_value)) {
    return InvalidArgumentErrorBuilder()
           << "Invalid value for type: " << type_value;
  }
  ir_update.set_type((p4::v1::Update_Type)type_value);

  ASSIGN_OR_RETURN(const auto *table_entry,
                   GetMessageField(update, "table_entry"));
  ASSIGN_OR_RETURN(*ir_update.mutable_table_entry(),
                   PdTableEntryToIr(info, *table_entry));
  return ir_update;
}

absl::Status IrWriteRequestToPd(const IrP4Info &info, const IrWriteRequest &ir,
                                google::protobuf::Message *write_request) {
  RETURN_IF_ERROR(SetUint64Field(write_request, "device_id", ir.device_id()));
  if (ir.election_id().high() > 0 || ir.election_id().low() > 0) {
    ASSIGN_OR_RETURN(auto *election_id,
                     GetMutableMessage(write_request, "election_id"));
    RETURN_IF_ERROR(
        SetUint64Field(election_id, "high", ir.election_id().high()));
    RETURN_IF_ERROR(SetUint64Field(election_id, "low", ir.election_id().low()));
  }

  ASSIGN_OR_RETURN(const auto updates_descriptor,
                   GetFieldDescriptor(*write_request, "updates"));
  for (const auto &ir_update : ir.updates()) {
    RETURN_IF_ERROR(IrUpdateToPd(info, ir_update,
                                 write_request->GetReflection()->AddMessage(
                                     write_request, updates_descriptor)));
  }
  return absl::OkStatus();
}

absl::StatusOr<IrWriteRequest> PdWriteRequestToIr(
    const IrP4Info &info, const google::protobuf::Message &write_request) {
  IrWriteRequest ir_write_request;
  ASSIGN_OR_RETURN(const auto &device_id,
                   GetUint64Field(write_request, "device_id"));
  ir_write_request.set_device_id(device_id);

  ASSIGN_OR_RETURN(const auto *election_id,
                   GetMessageField(write_request, "election_id"));
  ASSIGN_OR_RETURN(const auto &high, GetUint64Field(*election_id, "high"));
  ASSIGN_OR_RETURN(const auto &low, GetUint64Field(*election_id, "low"));
  if (high > 0 || low > 0) {
    auto *ir_election_id = ir_write_request.mutable_election_id();
    ir_election_id->set_high(high);
    ir_election_id->set_low(low);
  }

  ASSIGN_OR_RETURN(const auto updates_descriptor,
                   GetFieldDescriptor(write_request, "updates"));
  for (auto i = 0; i < write_request.GetReflection()->FieldSize(
                           write_request, updates_descriptor);
       ++i) {
    ASSIGN_OR_RETURN(
        *ir_write_request.add_updates(),
        PdUpdateToIr(info, write_request.GetReflection()->GetRepeatedMessage(
                               write_request, updates_descriptor, i)));
  }

  return ir_write_request;
}

// Converts all IR matches to their PD form and stores them in the match field
// of the PD table entry.
static absl::Status IrMatchEntryToPd(const IrTableDefinition &ir_table_info,
                                     const IrTableEntry &ir_table_entry,
                                     google::protobuf::Message *pd_match) {
  for (const auto &ir_match : ir_table_entry.matches()) {
    ASSIGN_OR_RETURN(const auto &ir_match_info,
                     gutil::FindOrStatus(ir_table_info.match_fields_by_name(),
                                         ir_match.name()),
                     _ << "P4Info for table \""
                       << ir_table_info.preamble().name()
                       << "\" does not contain match with name \""
                       << ir_match.name() << "\".");
    switch (ir_match_info.match_field().match_type()) {
      case MatchField::EXACT: {
        ASSIGN_OR_RETURN(
            const auto pd_value,
            IrValueToFormattedString(ir_match.exact(), ir_match_info.format()));
        RETURN_IF_ERROR(SetStringField(pd_match, ir_match.name(), pd_value));
        break;
      }
      case MatchField::LPM: {
        ASSIGN_OR_RETURN(auto *pd_lpm,
                         GetMutableMessage(pd_match, ir_match.name()));
        ASSIGN_OR_RETURN(const auto pd_value,
                         IrValueToFormattedString(ir_match.lpm().value(),
                                                  ir_match_info.format()));
        RETURN_IF_ERROR(SetStringField(pd_lpm, "value", pd_value));
        RETURN_IF_ERROR(SetInt32Field(pd_lpm, "prefix_length",
                                      ir_match.lpm().prefix_length()));
        break;
      }
      case MatchField::TERNARY: {
        ASSIGN_OR_RETURN(auto *pd_ternary,
                         GetMutableMessage(pd_match, ir_match.name()));
        ASSIGN_OR_RETURN(const auto pd_value,
                         IrValueToFormattedString(ir_match.ternary().value(),
                                                  ir_match_info.format()));
        RETURN_IF_ERROR(SetStringField(pd_ternary, "value", pd_value));
        ASSIGN_OR_RETURN(const auto pd_mask,
                         IrValueToFormattedString(ir_match.ternary().mask(),
                                                  ir_match_info.format()));
        RETURN_IF_ERROR(SetStringField(pd_ternary, "mask", pd_mask));
        break;
      }
      case MatchField::OPTIONAL: {
        ASSIGN_OR_RETURN(auto *pd_optional,
                         GetMutableMessage(pd_match, ir_match.name()));
        ASSIGN_OR_RETURN(const auto pd_value,
                         IrValueToFormattedString(ir_match.optional().value(),
                                                  ir_match_info.format()));
        RETURN_IF_ERROR(SetStringField(pd_optional, "value", pd_value));
        break;
      }
      default:
        return gutil::InvalidArgumentErrorBuilder()
               << "Unsupported match type \""
               << MatchField_MatchType_Name(
                      ir_match_info.match_field().match_type())
               << "\" in \"" << ir_match.name() << "\".";
    }
  }
  return absl::OkStatus();
}

// Converts all PD matches to their IR form and stores them in the matches field
// of ir_table_entry.
static absl::Status PdMatchEntryToIr(const IrTableDefinition &ir_table_info,
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
      case MatchField::OPTIONAL: {
        auto *ir_optional = ir_match->mutable_optional();
        ASSIGN_OR_RETURN(const auto *pd_optional,
                         GetMessageField(pd_match, pd_match_name));

        ASSIGN_OR_RETURN(const auto &pd_value,
                         GetStringField(*pd_optional, "value"));
        ASSIGN_OR_RETURN(
            *ir_optional->mutable_value(),
            FormattedStringToIrValue(pd_value, ir_match_info.format()));
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

// Converts an IR action invocation to its PD form and stores it in the parent
// message.
static absl::Status IrActionInvocationToPd(
    const IrP4Info &ir_p4info, const IrActionInvocation &ir_action,
    google::protobuf::Message *parent_message) {
  ASSIGN_OR_RETURN(
      const auto &ir_action_info,
      gutil::FindOrStatus(ir_p4info.actions_by_name(), ir_action.name()),
      _ << "P4Info does not contain action with name \"" << ir_action.name()
        << "\".");
  ASSIGN_OR_RETURN(const auto &pd_action_name,
                   P4NameToProtobufFieldName(ir_action.name(), kP4Action));
  ASSIGN_OR_RETURN(auto *pd_action,
                   GetMutableMessage(parent_message, pd_action_name));
  for (const auto &ir_param : ir_action.params()) {
    ASSIGN_OR_RETURN(
        const auto &param_info,
        gutil::FindOrStatus(ir_action_info.params_by_name(), ir_param.name()));
    ASSIGN_OR_RETURN(
        const auto &pd_value,
        IrValueToFormattedString(ir_param.value(), param_info.format()));
    RETURN_IF_ERROR(SetStringField(pd_action, ir_param.name(), pd_value));
  }
  return absl::OkStatus();
}

// Converts a PD action invocation to its IR form and returns it.
static absl::StatusOr<IrActionInvocation> PdActionInvocationToIr(
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

// Converts an IR action set to its PD form and stores it in the
// PD table entry.
static absl::Status IrActionSetToPd(const IrP4Info &ir_p4info,
                                    const IrTableEntry &ir_table_entry,
                                    google::protobuf::Message *pd_table) {
  ASSIGN_OR_RETURN(const auto *pd_action_set_descriptor,
                   GetFieldDescriptor(*pd_table, "actions"));
  for (const auto &ir_action_set_invocation :
       ir_table_entry.action_set().actions()) {
    auto *pd_action_set = pd_table->GetReflection()->AddMessage(
        pd_table, pd_action_set_descriptor);
    RETURN_IF_ERROR(IrActionInvocationToPd(
        ir_p4info, ir_action_set_invocation.action(), pd_action_set));
    RETURN_IF_ERROR(SetInt32Field(pd_action_set, "weight",
                                  ir_action_set_invocation.weight()));
  }
  return absl::OkStatus();
}

// Converts a PD action set to its IR form and stores it in the
// ir_table_entry.
static absl::StatusOr<IrActionSetInvocation> PdActionSetToIr(
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

absl::Status IrTableEntryToPd(const IrP4Info &ir_p4info, const IrTableEntry &ir,
                              google::protobuf::Message *pd) {
  ASSIGN_OR_RETURN(
      const auto &ir_table_info,
      gutil::FindOrStatus(ir_p4info.tables_by_name(), ir.table_name()),
      _ << "Table \"" << ir.table_name() << "\" does not exist in P4Info. "
        << kPdProtoAndP4InfoOutOfSync);
  ASSIGN_OR_RETURN(const auto pd_table_name,
                   P4NameToProtobufFieldName(ir.table_name(), kP4Table));
  ASSIGN_OR_RETURN(auto *pd_table, GetMutableMessage(pd, pd_table_name));

  ASSIGN_OR_RETURN(auto *pd_match, GetMutableMessage(pd_table, "match"));
  RETURN_IF_ERROR(IrMatchEntryToPd(ir_table_info, ir, pd_match));

  if (ir.priority() != 0) {
    RETURN_IF_ERROR(SetInt32Field(pd_table, "priority", ir.priority()));
  }

  if (ir_table_info.uses_oneshot()) {
    RETURN_IF_ERROR(IrActionSetToPd(ir_p4info, ir, pd_table));
  } else {
    ASSIGN_OR_RETURN(auto *pd_action, GetMutableMessage(pd_table, "action"));
    RETURN_IF_ERROR(IrActionInvocationToPd(ir_p4info, ir.action(), pd_action));
  }

  if (ir_table_info.has_meter()) {
    ASSIGN_OR_RETURN(auto *config, GetMutableMessage(pd_table, "meter_config"));
    const auto &ir_meter_config = ir.meter_config();
    if (ir_meter_config.cir() != ir_meter_config.pir()) {
      return InvalidArgumentErrorBuilder()
             << "CIR and PIR values should be equal. Got CIR as "
             << ir_meter_config.cir() << ", PIR as " << ir_meter_config.pir()
             << ".";
    }
    if (ir_meter_config.cburst() != ir_meter_config.pburst()) {
      return InvalidArgumentErrorBuilder()
             << "CBurst and PBurst values should be equal. Got CBurst as "
             << ir_meter_config.cburst() << ", PBurst as "
             << ir_meter_config.pburst() << ".";
    }
    switch (ir_table_info.meter().unit()) {
      case p4::config::v1::MeterSpec_Unit_BYTES: {
        RETURN_IF_ERROR(
            SetInt64Field(config, "bytes_per_second", ir_meter_config.cir()));
        RETURN_IF_ERROR(
            SetInt64Field(config, "burst_bytes", ir_meter_config.cburst()));
        break;
      }
      case p4::config::v1::MeterSpec_Unit_PACKETS: {
        RETURN_IF_ERROR(
            SetInt64Field(config, "packets_per_second", ir_meter_config.cir()));
        RETURN_IF_ERROR(
            SetInt64Field(config, "burst_packets", ir_meter_config.cburst()));
        break;
      }
      default:
        return InvalidArgumentErrorBuilder()
               << "Invalid meter unit: " << ir_table_info.meter().unit();
    }
  }

  if (ir_table_info.has_counter()) {
    switch (ir_table_info.counter().unit()) {
      case p4::config::v1::CounterSpec_Unit_BYTES: {
        RETURN_IF_ERROR(SetInt64Field(pd_table, "byte_counter",
                                      ir.counter_data().byte_count()));
        break;
      }
      case p4::config::v1::CounterSpec_Unit_PACKETS: {
        RETURN_IF_ERROR(SetInt64Field(pd_table, "packet_counter",
                                      ir.counter_data().packet_count()));
        break;
      }
      case p4::config::v1::CounterSpec_Unit_BOTH: {
        RETURN_IF_ERROR(SetInt64Field(pd_table, "byte_counter",
                                      ir.counter_data().byte_count()));
        RETURN_IF_ERROR(SetInt64Field(pd_table, "packet_counter",
                                      ir.counter_data().packet_count()));
        break;
      }
      default:
        return InvalidArgumentErrorBuilder()
               << "Invalid counter unit: " << ir_table_info.meter().unit();
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<IrTableEntry> PdTableEntryToIr(
    const IrP4Info &ir_p4info, const google::protobuf::Message &pd) {
  IrTableEntry ir;
  ASSIGN_OR_RETURN(const std::string &pd_table_field_name,
                   gutil::GetOneOfFieldName(pd, "entry"));
  ASSIGN_OR_RETURN(const std::string &p4_table_name,
                   ProtobufFieldNameToP4Name(pd_table_field_name, kP4Table));
  ASSIGN_OR_RETURN(
      const auto &ir_table_info,
      gutil::FindOrStatus(ir_p4info.tables_by_name(), p4_table_name),
      _ << "Table \"" << p4_table_name << "\" does not exist in P4Info. "
        << kPdProtoAndP4InfoOutOfSync);
  ir.set_table_name(p4_table_name);

  ASSIGN_OR_RETURN(const auto *pd_table,
                   GetMessageField(pd, pd_table_field_name));

  ASSIGN_OR_RETURN(const auto *pd_match, GetMessageField(*pd_table, "match"));
  RETURN_IF_ERROR(PdMatchEntryToIr(ir_table_info, *pd_match, &ir));

  const auto &status_or_priority = GetInt32Field(*pd_table, "priority");
  if (status_or_priority.ok()) {
    ir.set_priority(status_or_priority.value());
  }

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
      case p4::config::v1::CounterSpec_Unit_BYTES: {
        ASSIGN_OR_RETURN(const auto &pd_byte_counter,
                         GetInt64Field(*pd_table, "byte_counter"));
        ir.mutable_counter_data()->set_byte_count(pd_byte_counter);
        break;
      }
      case p4::config::v1::CounterSpec_Unit_PACKETS: {
        ASSIGN_OR_RETURN(const auto &pd_packet_counter,
                         GetInt64Field(*pd_table, "packet_counter"));
        ir.mutable_counter_data()->set_packet_count(pd_packet_counter);
        break;
      }
      case p4::config::v1::CounterSpec_Unit_BOTH: {
        ASSIGN_OR_RETURN(const auto &pd_byte_counter,
                         GetInt64Field(*pd_table, "byte_counter"));
        ir.mutable_counter_data()->set_byte_count(pd_byte_counter);
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

// Generic helper that works for both packet-in and packet-out. For both, T is
// one of {IrPacketIn, IrPacketOut}.
template <typename T>
absl::StatusOr<T> PdPacketIoToIr(const IrP4Info &info, const std::string &kind,
                                 const google::protobuf::Message &packet) {
  T result;
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(packet, "payload"));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BYTES));
  const auto &pd_payload =
      packet.GetReflection()->GetString(packet, field_descriptor);
  result.set_payload(pd_payload);

  google::protobuf::Map<std::string, IrPacketIoMetadataDefinition>
      metadata_by_name;
  if (kind == "packet-in") {
    metadata_by_name = info.packet_in_metadata_by_name();
  } else if (kind == "packet-out") {
    metadata_by_name = info.packet_out_metadata_by_name();
  } else {
    return InvalidArgumentErrorBuilder() << "Invalid PacketIo type " << kind;
  }

  ASSIGN_OR_RETURN(const auto &pd_metadata,
                   GetMessageField(packet, "metadata"));
  for (const auto &entry : Ordered(metadata_by_name)) {
    ASSIGN_OR_RETURN(const auto &pd_entry,
                     GetStringField(*pd_metadata, entry.first));
    auto *ir_metadata = result.add_metadata();
    ir_metadata->set_name(entry.first);
    ASSIGN_OR_RETURN(*ir_metadata->mutable_value(),
                     FormattedStringToIrValue(pd_entry, entry.second.format()));
  }

  return result;
}

template <typename T>
absl::Status IrPacketIoToPd(const IrP4Info &info, const std::string &kind,
                            const T &packet,
                            google::protobuf::Message *pd_packet) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptor(*pd_packet, "payload"));
  RETURN_IF_ERROR(ValidateFieldDescriptorType(field_descriptor,
                                              FieldDescriptor::TYPE_BYTES));
  pd_packet->GetReflection()->SetString(pd_packet, field_descriptor,
                                        packet.payload());

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

    ASSIGN_OR_RETURN(const auto &metadata_definition,
                     gutil::FindOrStatus(metadata_by_name, name),
                     _ << "\"" << kind << "\" metadata with name \"" << name
                       << "\" not defined.");
    ASSIGN_OR_RETURN(const auto &raw_value,
                     IrValueToFormattedString(metadata.value(),
                                              metadata_definition.format()));
    ASSIGN_OR_RETURN(auto *pd_metadata,
                     GetMutableMessage(pd_packet, "metadata"));
    RETURN_IF_ERROR(SetStringField(pd_metadata, name, raw_value));
  }
  return absl::OkStatus();
}

absl::StatusOr<IrPacketIn> PdPacketInToIr(
    const IrP4Info &info, const google::protobuf::Message &packet) {
  return PdPacketIoToIr<IrPacketIn>(info, "packet-in", packet);
}
absl::StatusOr<IrPacketOut> PdPacketOutToIr(
    const IrP4Info &info, const google::protobuf::Message &packet) {
  return PdPacketIoToIr<IrPacketOut>(info, "packet-out", packet);
}

absl::Status IrPacketInToPd(const IrP4Info &info, const IrPacketIn &packet,
                            google::protobuf::Message *pd_packet) {
  return IrPacketIoToPd<IrPacketIn>(info, "packet-in", packet, pd_packet);
}
absl::Status IrPacketOutToPd(const IrP4Info &info, const IrPacketOut &packet,
                             google::protobuf::Message *pd_packet) {
  return IrPacketIoToPd<IrPacketOut>(info, "packet-out", packet, pd_packet);
}

static absl::Status IrUpdateStatusToPd(
    const IrUpdateStatus &ir_update_status,
    google::protobuf::Message *pd_update_status) {
  RETURN_IF_ERROR(
      SetEnumField(pd_update_status, "code", ir_update_status.code()));
  RETURN_IF_ERROR(
      SetStringField(pd_update_status, "message", ir_update_status.message()));
  return absl::OkStatus();
}

static absl::Status IrWriteResponseToPd(
    const IrWriteResponse &ir_write_response,
    google::protobuf::Message *pd_rpc_response) {
  // Iterates through each ir update status and add message to pd via
  // AddRepeatedMutableMessage
  for (const IrUpdateStatus &ir_update_status : ir_write_response.statuses()) {
    ASSIGN_OR_RETURN(auto *pd_update_status,
                     AddRepeatedMutableMessage(pd_rpc_response, "statuses"));
    RETURN_IF_ERROR(IrUpdateStatusToPd(ir_update_status, pd_update_status));
  }
  return absl::OkStatus();
}

absl::Status IrWriteRpcStatusToPd(const IrWriteRpcStatus &ir_write_status,
                                  google::protobuf::Message *pd) {
  switch (ir_write_status.status_case()) {
    case IrWriteRpcStatus::kRpcResponse: {
      ASSIGN_OR_RETURN(auto *pd_rpc_response,
                       GetMutableMessage(pd, "rpc_response"));
      return IrWriteResponseToPd(ir_write_status.rpc_response(),
                                 pd_rpc_response);
    }
    case IrWriteRpcStatus::kRpcWideError: {
      ASSIGN_OR_RETURN(auto *pd_rpc_wide_error,
                       GetMutableMessage(pd, "rpc_wide_error"));
      RETURN_IF_ERROR(SetInt32Field(pd_rpc_wide_error, "code",
                                    ir_write_status.rpc_wide_error().code()));
      RETURN_IF_ERROR(
          SetStringField(pd_rpc_wide_error, "message",
                         ir_write_status.rpc_wide_error().message()));
      break;
    }
    default:
      return absl::UnknownError("Unknown IrWriteRpcStatus case.");
  }
  return absl::OkStatus();
}

static absl::StatusOr<IrUpdateStatus> PdUpdateStatusToIr(
    const google::protobuf::Message &pd) {
  IrUpdateStatus ir_update_status;
  ASSIGN_OR_RETURN(int google_rpc_code, GetEnumField(pd, "code"));
  ASSIGN_OR_RETURN(std::string update_status_message,
                   GetStringField(pd, "message"));
  ir_update_status.set_code(static_cast<google::rpc::Code>(google_rpc_code));
  ir_update_status.set_message(update_status_message);
  return ir_update_status;
}

static absl::StatusOr<IrWriteResponse> PdWriteResponseToIr(
    const google::protobuf::Message &pd) {
  IrWriteResponse ir_write_response;
  ASSIGN_OR_RETURN(const auto *status_message,
                   GetMessageField(pd, "rpc_response"));
  ASSIGN_OR_RETURN(const auto *repeated_update_status_field_descriptor,
                   GetFieldDescriptor(*status_message, "statuses"));
  for (int i = 0;
       i < status_message->GetReflection()->FieldSize(
               *status_message, repeated_update_status_field_descriptor);
       i++) {
    // Extract out the Pd::UpdateStatus and pass to PdUpdateStatusToIr
    ASSIGN_OR_RETURN(const auto *pd_update_status,
                     GetRepeatedMessage(*status_message, "statuses", i));
    ASSIGN_OR_RETURN(const auto ir_update_status,
                     PdUpdateStatusToIr(*pd_update_status));
    *ir_write_response.add_statuses() = ir_update_status;
  }
  return ir_write_response;
}

absl::StatusOr<IrWriteRpcStatus> PdWriteRpcStatusToIr(
    const google::protobuf::Message &pd) {
  IrWriteRpcStatus ir_write_rpc_status;
  ASSIGN_OR_RETURN(std::string status_oneof_name,
                   gutil::GetOneOfFieldName(pd, "status"));
  // status_message is of type WriteResponse with field name rpc_response
  if (status_oneof_name == "rpc_response") {
    ASSIGN_OR_RETURN(*ir_write_rpc_status.mutable_rpc_response(),
                     PdWriteResponseToIr(pd));
  } else if (status_oneof_name == "rpc_wide_error") {
    ASSIGN_OR_RETURN(const auto *rpc_wide_error_message,
                     GetMessageField(pd, "rpc_wide_error"));
    ASSIGN_OR_RETURN(int32_t status_code,
                     GetInt32Field(*rpc_wide_error_message, "code"));
    ASSIGN_OR_RETURN(std::string status_message,
                     GetStringField(*rpc_wide_error_message, "message"));
    auto *rpc_wide_error = ir_write_rpc_status.mutable_rpc_wide_error();
    rpc_wide_error->set_code(status_code);
    rpc_wide_error->set_message(status_message);
    return ir_write_rpc_status;
  } else {
    return gutil::InvalidArgumentErrorBuilder()
           << status_oneof_name << " is not a valid status one_of value. "
           << kPdProtoAndP4InfoOutOfSync;
  }
  return ir_write_rpc_status;
}

}  // namespace pdpi
