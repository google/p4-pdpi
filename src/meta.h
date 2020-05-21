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

#ifndef PDPI_META_H
#define PDPI_META_H
// P4 metadata representation definitions for use in conversion to and from
// Program-Independent to either Program-Dependent or App-DB formats

#include "p4/v1/p4runtime.pb.h"

#include "src/util.h"

namespace pdpi {

struct P4MatchFieldMetadata {
  p4::config::v1::MatchField match_field;
  Format format;
};

struct P4TableMetadata {
  p4::config::v1::Preamble preamble;
  // Maps match field IDs to match fields.
  absl::flat_hash_map<uint32_t, P4MatchFieldMetadata> match_fields;
  uint32_t num_mandatory_match_fields;
  absl::flat_hash_set<uint32_t> valid_actions;
  uint32_t size;
};

struct P4ActionParamMetadata {
  p4::config::v1::Action::Param param;
  Format format;
};

struct P4ActionMetadata {
  p4::config::v1::Preamble preamble;
  // Maps param IDs to params.
  absl::flat_hash_map<uint32_t, P4ActionParamMetadata> params;
};

struct P4InfoMetadata {
  // Maps table IDs to table metadata.
  absl::flat_hash_map<uint32_t, P4TableMetadata> tables;
  // Maps action IDs to actions.
  absl::flat_hash_map<uint32_t, P4ActionMetadata> actions;
};

// Creates the metadata needed for conversion from PI to PD and viceversa.
P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info);
// Returns the metadata as a string.
std::string MetadataToString(const P4InfoMetadata &metadata);
}  // namespace pdpi
#endif  // PDPI_META_H
