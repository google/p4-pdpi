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

#include "p4_pdpi/pdgenlib.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/map.h"
#include "gutil/collections.h"
#include "gutil/status.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4_pdpi/internal/ordered_protobuf_map.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/pd.h"

using ::absl::StatusOr;
using ::gutil::InvalidArgumentErrorBuilder;
using ::p4::config::v1::MatchField;

namespace pdpi {

namespace {

// Returns a P4 object ID without the object tag.
uint32_t IdWithoutTag(uint32_t id) { return id & 0xffffff; }

// Returns a header comment.
std::string HeaderComment(const std::string& title) {
  std::string prefix = "// -- ";
  constexpr int kLineWidth = 80;
  std::string postfix(kLineWidth - prefix.size() - title.size() - 1, '-');
  return absl::StrCat("\n", prefix, title, " ", postfix, "\n");
}

// Returns a comment explaining the format of a match field or parameter, e.g.
// "Format::HEX_STRING / 10 bits".
std::string GetFormatComment(Format format, int32_t bitwidth) {
  std::string bitwidth_str = "";
  if (format == Format::HEX_STRING) {
    bitwidth_str = absl::StrCat(" / ", bitwidth, " bits");
  }
  return absl::StrCat("Format::", Format_Name(format), bitwidth_str);
}

// Returns the proto field for a match.
StatusOr<std::string> GetMatchFieldDeclaration(
    const IrMatchFieldDefinition& match) {
  std::string type;
  std::string match_kind;

  switch (match.match_field().match_type()) {
    case MatchField::TERNARY:
      type = "Ternary";
      match_kind = "ternary";
      break;
    case MatchField::EXACT:
      type = "string";
      match_kind = "exact";
      break;
    case MatchField::OPTIONAL:
      type = "Optional";
      match_kind = "optional";
      break;
    case MatchField::LPM:
      type = "Lpm";
      match_kind = "lpm";
      break;
    default:
      return InvalidArgumentErrorBuilder()
             << "Invalid match kind: " << match.DebugString();
  }

  ASSIGN_OR_RETURN(
      const std::string field_name,
      P4NameToProtobufFieldName(match.match_field().name(), kP4MatchField));
  return absl::StrCat(
      type, " ", field_name, " = ", match.match_field().id(), "; // ",
      match_kind, " match / ",
      GetFormatComment(match.format(), match.match_field().bitwidth()));
}

// Returns the nested Match message for a given table.
StatusOr<std::string> GetTableMatchMessage(const IrTableDefinition& table) {
  std::string result = "";

  absl::StrAppend(&result, "  message Match {\n");
  std::vector<IrMatchFieldDefinition> match_fields;
  for (const auto& [id, match] : Ordered(table.match_fields_by_id())) {
    match_fields.push_back(match);
  }
  std::sort(match_fields.begin(), match_fields.end(),
            [](const IrMatchFieldDefinition& a,
               const IrMatchFieldDefinition& b) -> bool {
              return a.match_field().id() < b.match_field().id();
            });
  for (const auto& match : match_fields) {
    ASSIGN_OR_RETURN(const auto& match_pd, GetMatchFieldDeclaration(match));
    absl::StrAppend(&result, "    ", match_pd, "\n");
  }
  absl::StrAppend(&result, "  }\n");

  return result;
}

// Returns the nested Action message for a given table.
StatusOr<std::string> GetTableActionMessage(const IrTableDefinition& table) {
  std::string result;

  absl::StrAppend(&result, "  message Action {\n");
  std::vector<IrActionReference> entry_actions;
  for (const auto& action : table.entry_actions()) {
    entry_actions.push_back(action);
  }
  std::sort(entry_actions.begin(), entry_actions.end(),
            [](const IrActionReference& a, const IrActionReference& b) -> bool {
              return a.action().preamble().id() < b.action().preamble().id();
            });
  if (entry_actions.size() > 1) {
    absl::StrAppend(&result, "  oneof action {\n");
  }
  absl::flat_hash_set<uint32_t> proto_ids;
  for (const auto& action : entry_actions) {
    const auto& name = action.action().preamble().alias();
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        proto_ids, action.proto_id(),
        absl::StrCat("Proto IDs for entry actions must be unique, but table ",
                     name, " has duplicate ID ", action.proto_id())));
    ASSIGN_OR_RETURN(const std::string action_message_name,
                     P4NameToProtobufMessageName(name, kP4Action));
    ASSIGN_OR_RETURN(const std::string action_field_name,
                     P4NameToProtobufFieldName(name, kP4Action));
    absl::StrAppend(&result, "    ", action_message_name, " ",
                    action_field_name, " = ", action.proto_id(), ";\n");
  }
  if (entry_actions.size() > 1) {
    absl::StrAppend(&result, "  }\n");
  }

  // If necessary, add weight.
  if (table.uses_oneshot()) {
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        proto_ids, table.weight_proto_id(),
        absl::StrCat("@weight_proto_id conflicts with the ID of an action")));
    absl::StrAppend(&result, "    int32 weight = ", table.weight_proto_id(),
                    ";\n");
    absl::StrAppend(&result, "  }\n");
  } else {
    absl::StrAppend(&result, "  }\n");
  }
  return result;
}

// Returns the message for a given table.
StatusOr<std::string> GetTableMessage(const IrTableDefinition& table) {
  std::string result = "";

  const std::string& name = table.preamble().alias();
  ASSIGN_OR_RETURN(const std::string message_name,
                   P4NameToProtobufMessageName(name, kP4Table));
  absl::StrAppend(&result, "message ", message_name, " {\n");

  // Match message.
  ASSIGN_OR_RETURN(const auto& match_message, GetTableMatchMessage(table));
  absl::StrAppend(&result, match_message);
  absl::StrAppend(&result, "  Match match = 1;\n");

  // Action message.
  ASSIGN_OR_RETURN(const auto& action_message, GetTableActionMessage(table));
  absl::StrAppend(&result, action_message);
  if (table.uses_oneshot()) {
    absl::StrAppend(&result, "  repeated Action actions = 2;\n");
  } else {
    absl::StrAppend(&result, "  Action action = 2;\n");
  }

  // Priority (if applicable).
  bool has_priority = false;
  for (const auto& [id, match] : Ordered(table.match_fields_by_id())) {
    const auto& kind = match.match_field().match_type();
    if (kind == MatchField::TERNARY || kind == MatchField::OPTIONAL ||
        kind == MatchField::RANGE) {
      has_priority = true;
    }
  }
  if (has_priority) {
    absl::StrAppend(&result, "  int32 priority = 3;\n");
  }

  // Meter (if applicable).
  if (table.has_meter()) {
    switch (table.meter().unit()) {
      case p4::config::v1::MeterSpec::BYTES:
        absl::StrAppend(&result, "  BytesMeterConfig meter_config = 4;\n");
        break;
      case p4::config::v1::MeterSpec::PACKETS:
        absl::StrAppend(&result, "  PacketsMeterConfig meter_config = 5;\n");
        break;
      default:
        return InvalidArgumentErrorBuilder()
               << "Unsupported meter: " << table.meter().DebugString();
    }
  }

  // Counter (if applicable).
  if (table.has_counter()) {
    switch (table.counter().unit()) {
      case p4::config::v1::CounterSpec::BYTES:
        absl::StrAppend(&result, "  int64 byte_counter = 6;\n");
        break;
      case p4::config::v1::CounterSpec::PACKETS:
        absl::StrAppend(&result, "  int64 packet_counter = 7;\n");
        break;
      case p4::config::v1::CounterSpec::BOTH:
        absl::StrAppend(&result, "  int64 byte_counter = 6;\n");
        absl::StrAppend(&result, "  int64 packet_counter = 7;\n");
        break;
      default:
        return InvalidArgumentErrorBuilder()
               << "Unsupported counter: " << table.counter().DebugString();
    }
  }

  absl::StrAppend(&result, "}");
  return result;
}

// Returns the message for the given action.
StatusOr<std::string> GetActionMessage(const IrActionDefinition& action) {
  std::string result = "";

  const std::string& name = action.preamble().alias();
  ASSIGN_OR_RETURN(const std::string message_name,
                   P4NameToProtobufMessageName(name, kP4Action));
  absl::StrAppend(&result, "message ", message_name, " {\n");

  // Sort parameters by ID
  std::vector<IrActionDefinition::IrActionParamDefinition> params;
  for (const auto& [id, param] : Ordered(action.params_by_id())) {
    params.push_back(param);
  }
  std::sort(params.begin(), params.end(),
            [](const IrActionDefinition::IrActionParamDefinition& a,
               const IrActionDefinition::IrActionParamDefinition& b) -> bool {
              return a.param().id() < b.param().id();
            });

  // Field for every param.
  for (const auto& param : params) {
    ASSIGN_OR_RETURN(
        const std::string param_name,
        P4NameToProtobufFieldName(param.param().name(), kP4Parameter));
    absl::StrAppend(
        &result, "  string ", param_name, " = ", param.param().id(), "; // ",
        GetFormatComment(param.format(), param.param().bitwidth()), "\n");
  }

  absl::StrAppend(&result, "}");
  return result;
}

StatusOr<std::string> GetPacketIoMessage(const IrP4Info& info) {
  std::string result = "";

  // Packet-in
  absl::StrAppend(&result, "message PacketIn {\n");
  absl::StrAppend(&result, "  bytes payload = 1;\n\n");
  absl::StrAppend(&result, "  message Metadata {\n");
  for (const auto& [name, meta] : Ordered(info.packet_in_metadata_by_name())) {
    ASSIGN_OR_RETURN(
        const std::string meta_name,
        P4NameToProtobufFieldName(meta.metadata().name(), kP4MetaField));
    absl::StrAppend(
        &result, "    string ", meta_name, " = ", meta.metadata().id(), "; // ",
        GetFormatComment(meta.format(), meta.metadata().bitwidth()), "\n");
  }
  absl::StrAppend(&result, "  }\n");
  absl::StrAppend(&result, "  Metadata metadata = 2;\n");
  absl::StrAppend(&result, "}\n");

  // Packet-out
  absl::StrAppend(&result, "message PacketOut {\n");
  absl::StrAppend(&result, "  bytes payload = 1;\n\n");
  absl::StrAppend(&result, "  message Metadata {\n");
  for (const auto& [name, meta] : Ordered(info.packet_out_metadata_by_name())) {
    ASSIGN_OR_RETURN(
        const std::string meta_name,
        P4NameToProtobufFieldName(meta.metadata().name(), kP4MetaField));
    absl::StrAppend(
        &result, "    string ", meta_name, " = ", meta.metadata().id(), "; // ",
        GetFormatComment(meta.format(), meta.metadata().bitwidth()), "\n");
  }
  absl::StrAppend(&result, "  }\n");
  absl::StrAppend(&result, "  Metadata metadata = 2;\n");
  absl::StrAppend(&result, "}");

  return result;
}

}  // namespace

StatusOr<std::string> IrP4InfoToPdProto(const IrP4Info& info,
                                        const std::string& package) {
  std::string result = "";

  // Header comment.
  absl::StrAppend(&result, R"(
// P4 PD proto

// NOTE: This file is automatically created from the P4 program, do not modify manually.

syntax = "proto3";
package )" + package + R"(;

import "p4/v1/p4runtime.proto";
import "google/rpc/code.proto";
import "google/rpc/status.proto";

// PDPI uses the following formats for different kinds of values:
// - Format::IPV4 for IPv4 addresses (32 bits), e.g., "10.0.0.1".
// - Format::IPV6 for IPv6 addresses (128 bits) formatted according to RFC 5952.
//   E.g. "2001:db8::1".
// - Format::MAC for MAC addresses (48 bits), e.g., "01:02:03:04:aa".
// - Format::STRING for entities that the controller refers to by string, e.g.,
//   ports.
// - Format::HEX_STRING for anything else, i.e. bitstrings of arbitrary length.
//   E.g., "0x01ab".

)");

  // General definitions.
  absl::StrAppend(&result, HeaderComment("General definitions"));
  absl::StrAppend(&result, R"(
// Ternary match. The value and mask are formatted according to the Format of the match field.
message Ternary {
  string value = 1;
  string mask = 2;
}

// LPM match. The value is formatted according to the Format of the match field.
message Lpm {
  string value = 1;
  int32 prefix_length = 2;
}

// Optional match. The value is formatted according to the Format of the match field.
message Optional {
  string value = 1;
}
)");

  // Sort tables by ID.
  std::vector<IrTableDefinition> tables;
  for (const auto& [id, table] : Ordered(info.tables_by_id())) {
    tables.push_back(table);
  }
  std::sort(tables.begin(), tables.end(),
            [](const IrTableDefinition& a, const IrTableDefinition& b) {
              return a.preamble().id() < b.preamble().id();
            });

  // Sort actions by ID.
  std::vector<IrActionDefinition> actions;
  for (const auto& [id, action] : Ordered(info.actions_by_id())) {
    actions.push_back(action);
  }
  std::sort(actions.begin(), actions.end(),
            [](const IrActionDefinition& a, const IrActionDefinition& b) {
              return a.preamble().id() < b.preamble().id();
            });

  // Table messages.
  absl::StrAppend(&result, HeaderComment("Tables"), "\n");
  for (const auto& table : tables) {
    ASSIGN_OR_RETURN(const auto& table_pd, GetTableMessage(table));
    absl::StrAppend(&result, table_pd, "\n\n");
  }

  // Action messages.
  absl::StrAppend(&result, HeaderComment("Actions"), "\n");
  for (const auto& action : actions) {
    ASSIGN_OR_RETURN(const auto& action_pd, GetActionMessage(action));
    absl::StrAppend(&result, action_pd, "\n\n");
  }

  // Overall TableEntry message.
  absl::StrAppend(&result, HeaderComment("All tables"), "\n");
  absl::StrAppend(&result, "message TableEntry {\n");
  absl::StrAppend(&result, "  oneof entry {\n");
  for (const auto& table : tables) {
    const auto& name = table.preamble().alias();
    ASSIGN_OR_RETURN(const std::string table_message_name,
                     P4NameToProtobufMessageName(name, kP4Table));
    ASSIGN_OR_RETURN(const std::string table_field_name,
                     P4NameToProtobufFieldName(name, kP4Table));
    absl::StrAppend(&result, "    ", table_message_name, " ", table_field_name,
                    " = ", IdWithoutTag(table.preamble().id()), ";\n");
  }
  absl::StrAppend(&result, "  }\n");
  absl::StrAppend(&result, "}\n\n");

  // PacketIo message.
  absl::StrAppend(&result, HeaderComment("Packet-IO"), "\n");
  ASSIGN_OR_RETURN(const auto& packetio_pd, GetPacketIoMessage(info));
  absl::StrAppend(&result, packetio_pd, "\n\n");

  // Meter messages.
  absl::StrAppend(&result, HeaderComment("Meter configs"));
  absl::StrAppend(&result, R"(
message BytesMeterConfig {
  // Committed/peak information rate (bytes per sec).
  int64 bytes_per_second = 1;
  // Committed/peak burst size.
  int64 burst_bytes = 2;
}

message PacketsMeterConfig {
  // Committed/peak information rate (packets per sec).
  int64 packets_per_second = 1;
  // Committed/peak burst size.
  int64 burst_packets = 2;
}
)");

  // RPC messages.
  absl::StrAppend(&result, HeaderComment("RPC messages"));
  absl::StrAppend(&result, R"(
// Describes an update in a Write RPC request.
message Update {
  // Required.
  p4.v1.Update.Type type = 1;
  // Required.
  TableEntry table_entry = 2;
}

// Describes a Write RPC request.
message WriteRequest {
  // Required.
  uint64 device_id = 1;
  // Required.
  p4.v1.Uint128 election_id = 2;
  // Required.
  repeated Update updates = 3;
}

// Describes the status of a single update in a Write RPC.
message UpdateStatus {
  // Required.
  google.rpc.Code code = 1;
  // Required for non-OK status.
  string message = 2;
}

// Describes the result of a Write RPC.
message WriteRpcStatus {
  oneof status {
    google.rpc.Status rpc_wide_error = 1;
    WriteResponse rpc_response = 2;
  }
}

// Describes a Write RPC response.
message WriteResponse {
  // Same order as `updates` in `WriteRequest`.
  repeated UpdateStatus statuses = 1;
}

// Read requests.
message ReadRequest {
  // Required.
  uint64 device_id = 1;
  // Indicates if counter data should be read.
  bool read_counter_data = 2;
  // Indicates if meter configs should be read.
  bool read_meter_configs = 3;
}

// A read request response.
message ReadResponse {
  // The table entries read by the switch.
  repeated TableEntry table_entries = 1;
}
)");

  return result;
}

}  // namespace pdpi
