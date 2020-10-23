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

#ifndef P4_PDPI_UTILS_IR_H
#define P4_PDPI_UTILS_IR_H

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/rpc/code.pb.h"
#include "gutil/status.h"
#include "p4_pdpi/ir.pb.h"
#if !defined(ABSL_IS_LITTLE_ENDIAN)
#error \
    "Some of the utility functions are not supported in Big Endian architecture"
#endif

namespace pdpi {

// There are 3 flavors of byte strings used in this file:
// 1. Normalized Byte String: The number of bytes in this string is the same as
//    the number of bytes defined in the bitwidth field of the element in the
//    P4Info file.
// 2. Canonical Byte String: This is the shortest string that fits the encoded
//    value. This is the format used by P4RT as described in
//    https://p4.org/p4runtime/spec/master/P4Runtime-Spec.html#sec-bytestrings.
// 3. Arbitrary Byte String: Any byte string encoding described by the P4RT
//    specification (specifically
//    https://p4.org/p4runtime/spec/master/P4Runtime-Spec.html#sec-bytestrings).
//    This can be the canonical representation, but it could also contain
//    additional leading zeros.
//
// Generally PDPI functions take arbitrary byte strings as inputs, and produce
// byte strings in canonical form as output (unless otherwise stated).

const uint32_t kNumBitsInByte = 8;
const uint32_t kNumBitsInMac = 48;
const uint32_t kNumBytesInMac = kNumBitsInMac / kNumBitsInByte;
const uint32_t kNumBitsInIpv4 = 32;
const uint32_t kNumBytesInIpv4 = kNumBitsInIpv4 / kNumBitsInByte;
const uint32_t kNumBitsInIpv6 = 128;
const uint32_t kNumBytesInIpv6 = kNumBitsInIpv6 / kNumBitsInByte;

// Returns the format for value, given the annotations on it, it's bitwidth
// and named type (if any).
absl::StatusOr<Format> GetFormat(const std::vector<std::string> &annotations,
                                 const int bitwidth, bool is_sdn_string);

// Checks if the IrValue in the IR table entry is in the same format as
// specified in the P4Info.
absl::Status ValidateIrValueFormat(const IrValue &ir_value,
                                   const Format &format);

// Converts the IR value to a PI byte string and returns it.
absl::StatusOr<std::string> IrValueToNormalizedByteString(
    const IrValue &ir_value, const int bitwidth);

// Converts the PI value to an IR value and returns it.
absl::StatusOr<IrValue> ArbitraryByteStringToIrValue(const Format &format,
                                                     const int bitwidth,
                                                     const std::string &bytes);

// Returns an IrValue based on a string value and a format. The value is
// expected to already be formatted correctly, and is just copied to the correct
// oneof field.
absl::StatusOr<IrValue> FormattedStringToIrValue(const std::string &value,
                                                 Format format);

// Returns a std::string based on an IrValue value and a format. The value is
// expected to already be formatted correctly, and is just returned as is.
absl::StatusOr<std::string> IrValueToFormattedString(const IrValue &value,
                                                     Format format);

// Returns a string of length ceil(expected_bitwidth/8).
absl::StatusOr<std::string> ArbitraryToNormalizedByteString(
    const std::string &bytes, int expected_bitwidth);

// Convert the given byte string into a uint value.
absl::StatusOr<uint64_t> ArbitraryByteStringToUint(const std::string &bytes,
                                                   int bitwidth);

// Convert the given uint to byte string.
absl::StatusOr<std::string> UintToNormalizedByteString(uint64_t value,
                                                       int bitwidth);

// Convert the given byte string into a : separated MAC representation.
// Input string should be 6 bytes long.
absl::StatusOr<std::string> NormalizedByteStringToMac(const std::string &bytes);

// Convert the given : separated MAC representation into a byte string.
absl::StatusOr<std::string> MacToNormalizedByteString(const std::string &mac);

// Convert the given byte string into a . separated IPv4 representation.
// Input should be 4 bytes long.
absl::StatusOr<std::string> NormalizedByteStringToIpv4(
    const std::string &bytes);

// Convert the given . separated IPv4 representation into a byte string.
absl::StatusOr<std::string> Ipv4ToNormalizedByteString(const std::string &ipv4);

// Convert the given byte string into a : separated IPv6 representation.
// Input should be 16 bytes long.
absl::StatusOr<std::string> NormalizedByteStringToIpv6(
    const std::string &bytes);

// Convert the given : separated IPv6 representation into a byte string.
absl::StatusOr<std::string> Ipv6ToNormalizedByteString(const std::string &ipv6);

// Convert a normalized byte string to its canonical form.
std::string NormalizedToCanonicalByteString(std::string bytes);

// Returns the number of bits used by the PI byte string interpreted as an
// unsigned integer.
uint32_t GetBitwidthOfByteString(const std::string &input_string);

// Returns if a (normalized) byte string is all zeros.
bool IsAllZeros(const std::string &s);

// Returns the intersection of two (normalized) byte strings.
absl::StatusOr<std::string> Intersection(const std::string &left,
                                         const std::string &right);

// Returns the (normalized) mask for a given prefix length.
absl::StatusOr<std::string> PrefixLenToMask(int prefix_len, int bitwidth);

bool RequiresPriority(const IrTableDefinition &ir_table_definition);

absl::Status IsGoogleRpcCode(int rpc_code);
// Checks if the rpc code and message satisfy the condition of UpdateStatus.
// 1: If `code` is ok, `message` should be empty.
// 2: If `code` is not ok, `message` should not be empty.
absl::Status ValidateGenericUpdateStatus(google::rpc::Code code,
                                         const std::string &message);
// Parses IrUpdateStatus inside of `ir_write_response`` into string.
std::string IrWriteResponseToReadableMessage(
    const IrWriteResponse &ir_write_response);

}  // namespace pdpi
#endif  // P4_PDPI_UTILS_IR_H
