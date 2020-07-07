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

#include "absl/container/flat_hash_set.h"
#include "gutil/collections.h"
#include "p4_pdpi/utils/pd.h"

using ::gutil::InvalidArgumentErrorBuilder;
using ::gutil::StatusOr;
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
      type = "string";
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

  ASSIGN_OR_RETURN(const std::string field_name,
                   P4NameToProtobufFieldName(match.match_field().name()));
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
  for (const auto& [id, match] : table.match_fields_by_id()) {
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
  std::vector<IrActionReference> actions;
  for (const auto& action : table.actions()) {
    // Skip default_only actions.
    if (action.ref().scope() == p4::config::v1::ActionRef::DEFAULT_ONLY) {
      continue;
    }
    actions.push_back(action);
  }
  std::sort(actions.begin(), actions.end(),
            [](const IrActionReference& a, const IrActionReference& b) -> bool {
              return a.action().preamble().id() < b.action().preamble().id();
            });
  if (actions.size() > 1) {
    absl::StrAppend(&result, "  oneof action {\n");
  }
  absl::flat_hash_set<uint32_t> proto_ids;
  for (const auto& action : actions) {
    const auto& name = action.action().preamble().name();
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        proto_ids, action.proto_id(),
        absl::StrCat("Proto IDs for actions must be unique, but table ", name,
                     " has duplicate ID ", action.proto_id(), ".")));
    ASSIGN_OR_RETURN(const std::string action_message_name,
                     P4NameToProtobufMessageName(name));
    ASSIGN_OR_RETURN(const std::string action_field_name,
                     P4NameToProtobufFieldName(name));
    absl::StrAppend(&result, "    ", action_message_name, " ",
                    action_field_name, " = ", action.proto_id(), ";\n");
  }
  if (actions.size() > 1) {
    absl::StrAppend(&result, "  }\n");
  }

  // If necessary, add weight.
  if (table.is_wcmp()) {
    RETURN_IF_ERROR(gutil::InsertIfUnique(
        proto_ids, table.weight_proto_id(),
        absl::StrCat("@weight_proto_id conflicts with the ID of an action.")));
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
                   P4NameToProtobufMessageName(name));
  absl::StrAppend(&result, "message ", message_name, " {\n");

  // Match message.
  ASSIGN_OR_RETURN(const auto& match_message, GetTableMatchMessage(table));
  absl::StrAppend(&result, match_message);
  absl::StrAppend(&result, "  Match match = 1;\n");

  // Action message.
  ASSIGN_OR_RETURN(const auto& action_message, GetTableActionMessage(table));
  absl::StrAppend(&result, action_message);
  if (table.is_wcmp()) {
    absl::StrAppend(&result, "  repeated Action actions = 2;\n");
  } else {
    absl::StrAppend(&result, "  Action action = 2;\n");
  }

  // Priority (if applicable).
  bool has_priority = false;
  for (const auto& [id, match] : table.match_fields_by_id()) {
    const auto& kind = match.match_field().match_type();
    if (kind == MatchField::TERNARY || kind == MatchField::OPTIONAL ||
        kind == MatchField::RANGE) {
      has_priority = true;
    }
  }
  if (has_priority) {
    absl::StrAppend(&result, "  int32 priority = 3;\n");
  }

  absl::StrAppend(&result, "}");
  return result;
}

// Returns the message for the given action.
StatusOr<std::string> GetActionMessage(const IrActionDefinition& action) {
  std::string result = "";

  const std::string& name = action.preamble().alias();
  ASSIGN_OR_RETURN(const std::string message_name,
                   P4NameToProtobufMessageName(name));
  absl::StrAppend(&result, "message ", message_name, "Action {\n");

  // Sort parameters by ID
  std::vector<IrActionDefinition::IrActionParamDefinition> params;
  for (const auto& [id, param] : action.params_by_id()) {
    params.push_back(param);
  }
  std::sort(params.begin(), params.end(),
            [](const IrActionDefinition::IrActionParamDefinition& a,
               const IrActionDefinition::IrActionParamDefinition& b) -> bool {
              return a.param().id() < b.param().id();
            });

  // Field for every param.
  for (const auto& param : params) {
    ASSIGN_OR_RETURN(const std::string param_name,
                     P4NameToProtobufFieldName(param.param().name()));
    absl::StrAppend(
        &result, "  string ", param_name, " = ", param.param().id(), "; // ",
        GetFormatComment(param.format(), param.param().bitwidth()), "\n");
  }

  absl::StrAppend(&result, "}");
  return result;
}

}  // namespace

StatusOr<std::string> IrP4InfoToPdProto(const IrP4Info& info) {
  std::string result = "";

  // Header comment.
  absl::StrAppend(&result, R"(
// P4 PD proto

// NOTE: This file is automatically created from the P4 program, do not modify manually.

syntax = "proto3";
package pdpi;

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
  string mask = 1;
}

// LPM match. The value is formatted according to the Format of the match field.
message Lpm {
  string value = 1;
  int32 prefix_length = 2;
}
)");

  // Sort tables by ID.
  std::vector<IrTableDefinition> tables;
  for (const auto& [id, table] : info.tables_by_id()) {
    tables.push_back(table);
  }
  std::sort(tables.begin(), tables.end(),
            [](const IrTableDefinition& a, const IrTableDefinition& b) {
              return a.preamble().id() < b.preamble().id();
            });

  // Sort actions by ID.
  std::vector<IrActionDefinition> actions;
  for (const auto& [id, action] : info.actions_by_id()) {
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
  absl::StrAppend(&result, HeaderComment("Action"), "\n");
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
                     P4NameToProtobufMessageName(name));
    ASSIGN_OR_RETURN(const std::string table_field_name,
                     P4NameToProtobufFieldName(name));
    absl::StrAppend(&result, "    ", table_message_name, " ", table_field_name,
                    " = ", IdWithoutTag(table.preamble().id()), ";\n");
  }
  absl::StrAppend(&result, "  }\n");
  absl::StrAppend(&result, "}\n\n");

  return result;
}

}  // namespace pdpi
