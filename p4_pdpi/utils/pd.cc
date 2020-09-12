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

#include "p4_pdpi/utils/pd.h"

#include <ctype.h>

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "gutil/proto.h"
#include "gutil/status.h"

namespace pdpi {

namespace {

constexpr absl::string_view kTableMessageSuffix = "Entry";
constexpr absl::string_view kActionMessageSuffix = "Action";
constexpr absl::string_view kTableFieldSuffix = "_entry";

constexpr absl::string_view ProtoMessageSuffix(P4EntityKind entity_kind) {
  switch (entity_kind) {
    case kP4Table:
      return kTableMessageSuffix;
    case kP4Action:
      return kActionMessageSuffix;
    default:
      return absl::string_view();
  }
}

constexpr absl::string_view ProtoFieldSuffix(P4EntityKind entity_kind) {
  switch (entity_kind) {
    case kP4Table:
      return kTableFieldSuffix;
    case kP4Action:  // Intentionally no suffix.
    default:
      return absl::string_view();
  }
}

// Converts snake_case to PascalCase.
std::string SnakeCaseToPascalCase(absl::string_view input) {
  std::string output;
  for (unsigned i = 0; i < input.size(); i += 1) {
    if (input[i] == '_') {
      i += 1;
      if (i < input.size()) {
        absl::StrAppend(&output, std::string(1, std::toupper(input[i])));
      }
    } else if (i == 0) {
      absl::StrAppend(&output, std::string(1, std::toupper(input[i])));
    } else {
      absl::StrAppend(&output, std::string(1, input[i]));
    }
  }
  return output;
}

}  // namespace

absl::StatusOr<std::string> P4NameToProtobufMessageName(
    absl::string_view p4_name, P4EntityKind entity_kind) {
  // TODO: validate the name is in snake case.
  const absl::string_view suffix = ProtoMessageSuffix(entity_kind);
  // Append suffix, unless it is redundant.
  return absl::StrCat(absl::StripSuffix(SnakeCaseToPascalCase(p4_name), suffix),
                      suffix);
}

absl::StatusOr<std::string> P4NameToProtobufFieldName(
    absl::string_view p4_name, P4EntityKind entity_kind) {
  // TODO: validate the name is in snake case.
  return absl::StrCat(p4_name, ProtoFieldSuffix(entity_kind));
}

absl::StatusOr<std::string> ProtobufFieldNameToP4Name(
    absl::string_view proto_field_name, P4EntityKind entity_kind) {
  // TODO: validate the name is in snake case.
  if (absl::ConsumeSuffix(&proto_field_name, ProtoFieldSuffix(entity_kind))) {
    return std::string(proto_field_name);
  }
  return gutil::InvalidArgumentErrorBuilder()
         << "expected field name '" << proto_field_name << "' to end in suffix "
         << ProtoFieldSuffix(entity_kind);
}

}  // namespace pdpi
