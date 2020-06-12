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

#ifndef PDPI_UTIL_H
#define PDPI_UTIL_H

#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/optional.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/status_utils.h"

namespace pdpi {

const uint32_t kNumBitsInByte = 8;
const uint32_t kNumBitsInMac = 48;
const uint32_t kNumBitsInIpv4 = 32;
const uint32_t kNumBitsInIpv6 = 128;
const uint32_t kNumBytesInIpv6 = kNumBitsInIpv6 / kNumBitsInByte;

constexpr char kPdProtoAndP4InfoOutOfSync[] =
    "The PD proto and P4Info file are out of sync.";

// Returns the format for value, given the annotations on it, it's bitwidth
// and named type (if any).
StatusOr<ir::Format> GetFormat(const std::vector<std::string> &annotations,
                               const int bitwidth,
                               const absl::optional<std::string> &named_type);

// Converts the PI value to an IR value and returns it.
StatusOr<ir::IrValue> FormatByteString(const ir::Format &format,
                                       const int bitwidth,
                                       const std::string &pi_value);

// Read the contents of the file into a protobuf.
absl::Status ReadProtoFromFile(const std::string &filename,
                               google::protobuf::Message *message);

// Read the contents of the string into a protobuf.
absl::Status ReadProtoFromString(const std::string &proto_string,
                                 google::protobuf::Message *message);

// Returns a string of length ceil(expected_bitwidth/8).
StatusOr<std::string> Normalize(const std::string &pi_byte_string,
                                int expected_bitwidth);

// Convert the given byte string into a uint value.
StatusOr<uint64_t> PiByteStringToUint(const std::string &pi_bytes,
                                      int bitwidth);

// Convert the given byte string into a : separated MAC representation
// Input string should have Normalize() called on it before being passed in.
std::string PiByteStringToMac(const std::string &normalized_bytes);

// Convert the given byte string into a . separated IPv4 representation
// Input string should have Normalize() called on it before being passed in.
std::string PiByteStringToIpv4(const std::string &normalized_bytes);

// Convert the given byte string into a : separated IPv6 representation
// Input string should have Normalize() called on it before being passed in.
std::string PiByteStringToIpv6(const std::string &normalized_bytes);

// Modify the p4_name in a way that is acceptable as fields in protobufs.
std::string ProtoFriendlyName(const std::string &p4_name);

// Return the name of the field in the PD table entry given the alias of the
// field.
std::string TableEntryFieldname(const std::string &alias);

// Return the name of the field of the PD action given the alias of the
// field.
std::string ActionFieldname(const std::string &alias);

// Return a mutable message given the name of the message field.
StatusOr<google::protobuf::Message *> GetMessageByFieldname(
    const std::string &fieldname, google::protobuf::Message *parent_message);

// Return the descriptor of the field to be used in the reflection API.
StatusOr<const google::protobuf::FieldDescriptor *> GetFieldDescriptorByName(
    const std::string &fieldname, google::protobuf::Message *parent_message);

// Returns the number of bits used by the PI byte string interpreted as an
// unsigned integer.
uint32_t GetBitwidthOfPiByteString(const std::string &input_string);

// Checks if the id is unique in set.
template <typename M>
absl::Status InsertIfUnique(absl::flat_hash_set<M> &set, const M &id,
                            const std::string &error_message) {
  const auto it = set.insert(id);
  if (!it.second) {
    return absl::Status(absl::StatusCode::kInvalidArgument, error_message);
  }

  return absl::OkStatus();
}
template <typename K, typename V>
absl::Status InsertIfUnique(absl::flat_hash_map<K, V> &map, K key, const V &val,
                            const std::string &error_message) {
  auto it = map.insert({key, val});
  if (!it.second) {
    return absl::Status(absl::StatusCode::kInvalidArgument, error_message);
  }

  return absl::OkStatus();
}
template <typename K, typename V>
absl::Status InsertIfUnique(google::protobuf::Map<K, V> *map, K key,
                            const V &val, const std::string &error_message) {
  auto it = map->insert({key, val});
  if (!it.second) {
    return absl::Status(absl::StatusCode::kInvalidArgument, error_message);
  }

  return absl::OkStatus();
}

// Returns map[key] if key exists in map.
template <typename M>
StatusOr<M> FindElement(const absl::flat_hash_map<uint32_t, M> &map,
                        uint32_t key, const std::string &error_message) {
  auto it = map.find(key);
  if (it == map.end()) {
    return absl::Status(absl::StatusCode::kInvalidArgument, error_message);
  }
  return it->second;
}
template <typename M>
StatusOr<M> FindElement(const google::protobuf::Map<uint32_t, M> &map,
                        uint32_t key, const std::string &error_message) {
  auto it = map.find(key);
  if (it == map.end()) {
    return absl::Status(absl::StatusCode::kInvalidArgument, error_message);
  }
  return it->second;
}

// Returns an escaped version of s, escaping non-printable characters and
// double quotes.
std::string EscapeString(const std::string &s);

}  // namespace pdpi
#endif  // PDPI_UTIL_H
