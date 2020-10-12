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

#ifndef GOOGLE_P4_PDPI_INTERNAL_ORDERED_PROTOBUF_MAP_H_
#define GOOGLE_P4_PDPI_INTERNAL_ORDERED_PROTOBUF_MAP_H_

#include "google/protobuf/map.h"

// Ordered view of an unordered protobuf Map. Useful for iterating over the map
// in deterministic fashion.
template <class Key, class Value>
std::map<Key, const Value&> Ordered(
    const google::protobuf::Map<Key, Value>& proto_map) {
  return std::map<Key, const Value&>(proto_map.begin(), proto_map.end());
}

#endif  // GOOGLE_P4_PDPI_INTERNAL_ORDERED_PROTOBUF_MAP_H_
