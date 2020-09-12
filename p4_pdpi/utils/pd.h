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

#ifndef P4_PDPI_UTILS_PD_H
#define P4_PDPI_UTILS_PD_H

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gutil/status.h"

namespace pdpi {

// The kinds of entities that can be declared in P4, e.g. tables and actions.
enum P4EntityKind {
  kP4Table,
  kP4Action,
  kP4Parameter,
  kP4MatchField,
  kP4MetaField,
};

// Given a P4 name for a given entity kind, returns the name of the
// corresponding protobuf message name.
absl::StatusOr<std::string> P4NameToProtobufMessageName(
    absl::string_view p4_name, P4EntityKind entity_kind);

// Given a P4 name for a given entity kind, returns the name of the
// corresponding protobuf field name.
absl::StatusOr<std::string> P4NameToProtobufFieldName(absl::string_view p4_name,
                                                      P4EntityKind entity_kind);

// Returns the inverse of `P4NameToProtobufFieldName`.
absl::StatusOr<std::string> ProtobufFieldNameToP4Name(
    absl::string_view proto_field_name, P4EntityKind entity_kind);

}  // namespace pdpi

#endif  // P4_PDPI_UTILS_PD_H
