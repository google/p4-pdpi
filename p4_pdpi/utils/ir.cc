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

#include "p4_pdpi/utils/ir.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "google/protobuf/map.h"
#include "google/rpc/code.pb.h"
#include "gutil/proto.h"
#include "gutil/status.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/config/v1/p4types.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

using ::pdpi::Format;
using ::pdpi::IrValue;

namespace {

bool IsValidMac(std::string s) {
  for (auto i = 0; i < 17; ++i) {
    if (i % 3 != 2 && (!isxdigit(s[i]) || absl::ascii_isupper(s[i]))) {
      return false;
    }
    if (i % 3 == 2 && s[i] != ':') {
      return false;
    }
  }
  return s.size() == 17;
}

bool IsValidIpv6(std::string s) {
  // This function checks extra requirements that are not covered by inet_ntop.
  for (int i = 0; i < s.size(); ++i) {
    if (s[i] == '.') {
      // TODO: Remove this when we find a way to get the inet_pton
      // library to ignore mixed mode IPv6 addresses
      continue;
    }
    if (s[i] == ':') {
      continue;
    }
    if (!isxdigit(s[i]) || absl::ascii_isupper(s[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

absl::StatusOr<std::string> ArbitraryToNormalizedByteString(
    const std::string &bytes, int expected_bitwidth) {
  std::string stripped_value = bytes;
  // Remove leading zeros
  stripped_value.erase(0, std::min(stripped_value.find_first_not_of('\x00'),
                                   stripped_value.size() - 1));
  int length = GetBitwidthOfByteString(stripped_value);
  if (length > expected_bitwidth) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Bytestring of length " << length << " bits does not fit in "
           << expected_bitwidth << " bits.";
  }

  int total_bytes;
  if (expected_bitwidth % 8) {
    total_bytes = expected_bitwidth / 8 + 1;
  } else {
    total_bytes = expected_bitwidth / 8;
  }
  std::string zeros =
      std::string(total_bytes - stripped_value.length(), '\x00');
  return zeros.append(stripped_value);
}

absl::StatusOr<uint64_t> ArbitraryByteStringToUint(const std::string &bytes,
                                                   int bitwidth) {
  if (bitwidth > 64) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("Cannot convert value with "
                                     "bitwidth ",
                                     bitwidth, " to uint."));
  }
  ASSIGN_OR_RETURN(const auto &stripped_value,
                   ArbitraryToNormalizedByteString(bytes, bitwidth));
  uint64_t nb_value;  // network byte order
  char value[sizeof(nb_value)];
  const int pad = static_cast<int>(sizeof(nb_value)) -
                  static_cast<int>(stripped_value.size());
  if (pad) {
    memset(value, 0, pad);
  }
  memcpy(value + pad, stripped_value.data(), stripped_value.size());
  memcpy(&nb_value, value, sizeof(nb_value));

  return be64toh(nb_value);
}

absl::StatusOr<std::string> UintToNormalizedByteString(uint64_t value,
                                                       int bitwidth) {
  if (bitwidth <= 0 || bitwidth > 64) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("Cannot convert value with "
                                     "bitwidth ",
                                     bitwidth, " to ByteString."));
  }
  std::string bytes = "";
  if (bitwidth <= 8) {
    uint8_t tmp = static_cast<uint8_t>(value);
    bytes.assign(reinterpret_cast<char *>(&tmp), sizeof(uint8_t));
  } else if (bitwidth <= 16) {
    uint16_t tmp = htons(static_cast<uint16_t>(value));
    bytes.assign(reinterpret_cast<char *>(&tmp), sizeof(uint16_t));
  } else if (bitwidth <= 32) {
    uint32_t tmp = htonl(static_cast<uint32_t>(value));
    bytes.assign(reinterpret_cast<char *>(&tmp), sizeof(uint32_t));
  } else {
    uint64_t tmp =
        (htonl(1) == 1)
            ? value
            : (static_cast<uint64_t>(htonl(value)) << 32) | htonl(value >> 32);
    bytes.assign(reinterpret_cast<char *>(&tmp), sizeof(uint64_t));
  }

  ASSIGN_OR_RETURN(auto normalized_str,
                   ArbitraryToNormalizedByteString(bytes, bitwidth));

  return normalized_str;
}

absl::StatusOr<std::string> NormalizedByteStringToMac(
    const std::string &bytes) {
  if (bytes.size() != kNumBytesInMac) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected length of input string to be " << kNumBytesInMac
           << ", but got " << bytes.size() << " instead.";
  }
  struct ether_addr byte_string;
  for (int i = 0; i < bytes.size(); ++i) {
    byte_string.ether_addr_octet[i] = bytes[i] & 0xFF;
  }
  std::string mac = std::string(ether_ntoa(&byte_string));
  std::vector<std::string> parts = absl::StrSplit(mac, ':');
  // ether_ntoa returns a string that is not zero padded. Add zero padding.
  for (int i = 0; i < parts.size(); ++i) {
    if (parts[i].size() == 1) {
      parts[i] = absl::StrCat("0", parts[i]);
    }
  }
  return absl::StrJoin(parts, ":");
}

absl::StatusOr<std::string> MacToNormalizedByteString(const std::string &mac) {
  if (!IsValidMac(mac)) {
    return gutil::InvalidArgumentErrorBuilder()
           << "String cannot be parsed as MAC address: " << mac
           << ". It must be of the format xx:xx:xx:xx:xx:xx where x is a lower "
              "case hexadecimal character.";
  }
  struct ether_addr *byte_string = ether_aton(mac.c_str());
  if (byte_string == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "String cannot be parsed as MAC address: " << mac;
  }
  return std::string((const char *)byte_string->ether_addr_octet,
                     sizeof(byte_string->ether_addr_octet));
}

absl::StatusOr<std::string> NormalizedByteStringToIpv4(
    const std::string &bytes) {
  if (bytes.size() != kNumBytesInIpv4) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected length of input string to be " << kNumBytesInIpv4
           << ", but got " << bytes.size() << " instead.";
  }
  char result[INET_ADDRSTRLEN];
  auto result_valid = inet_ntop(AF_INET, bytes.c_str(), result, sizeof(result));
  if (!result_valid) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Conversion of IPv4 address to string failed with error code: "
           << errno;
  }
  return std::string(result);
}

absl::StatusOr<std::string> Ipv4ToNormalizedByteString(
    const std::string &ipv4) {
  char ip_addr[kNumBytesInIpv4];
  if (inet_pton(AF_INET, ipv4.c_str(), &ip_addr) == 0) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Invalid IPv4 address: " << ipv4;
  }
  return std::string(ip_addr, kNumBytesInIpv4);
}

absl::StatusOr<std::string> NormalizedByteStringToIpv6(
    const std::string &bytes) {
  if (bytes.size() != kNumBytesInIpv6) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Expected length of input string to be " << kNumBytesInIpv6
           << ", but got " << bytes.size() << " instead.";
  }
  char result[INET6_ADDRSTRLEN];
  auto result_valid =
      inet_ntop(AF_INET6, bytes.c_str(), result, sizeof(result));
  if (!result_valid) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Conversion of IPv6 address to string failed with error code: "
           << errno;
  }
  return std::string(result);
}

absl::StatusOr<std::string> Ipv6ToNormalizedByteString(
    const std::string &ipv6) {
  if (!IsValidIpv6(ipv6)) {
    return gutil::InvalidArgumentErrorBuilder()
           << "String cannot be parsed as an IPv6 address. It must contain "
              "lower case hexadecimal characters.";
  }
  char ip6_addr[kNumBytesInIpv6];
  if (inet_pton(AF_INET6, ipv6.c_str(), &ip6_addr) == 0) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Invalid IPv6 address: " << ipv6;
  }
  return std::string(ip6_addr, kNumBytesInIpv6);
}

std::string NormalizedToCanonicalByteString(std::string bytes) {
  // Remove leading zeros
  std::string canonical = bytes;
  canonical.erase(
      0, std::min(canonical.find_first_not_of('\x00'), canonical.size() - 1));
  return canonical;
}

uint32_t GetBitwidthOfByteString(const std::string &input_string) {
  // Use str.length() - 1. MSB will need to be handled separately since it
  // can have leading zeros which should not be counted.
  int length_in_bits =
      (static_cast<int>(input_string.length()) - 1) * kNumBitsInByte;

  uint8_t msb = input_string[0];
  while (msb) {
    ++length_in_bits;
    msb >>= 1;
  }

  return length_in_bits;
}

absl::StatusOr<Format> GetFormat(const std::vector<std::string> &annotations,
                                 const int bitwidth, bool is_sdn_string) {
  Format format = Format::HEX_STRING;
  if (is_sdn_string) {
    format = Format::STRING;
  }
  for (const std::string &annotation : annotations) {
    if (absl::StartsWith(annotation, "@format(")) {
      if (format != Format::HEX_STRING) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Found conflicting formatting annotations.";
      }
      if (annotation == "@format(MAC_ADDRESS)") {
        format = Format::MAC;
      } else if (annotation == "@format(IPV4_ADDRESS)") {
        format = Format::IPV4;
      } else if (annotation == "@format(IPV6_ADDRESS)") {
        format = Format::IPV6;
      } else {
        return gutil::InvalidArgumentErrorBuilder()
               << "Found invalid format annotation: " << annotation;
      }
    }
  }
  if (format == Format::MAC && bitwidth != kNumBitsInMac) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Only 48 bit values can be formatted as a MAC address.";
  }
  if (format == Format::IPV4 && bitwidth != kNumBitsInIpv4) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Only 32 bit values can be formatted as an IPv4 address.";
  }
  if (format == Format::IPV6 && bitwidth != kNumBitsInIpv6) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Only 128 bit values can be formatted as an IPv6 address.";
  }
  return format;
}

absl::StatusOr<IrValue> ArbitraryByteStringToIrValue(const Format &format,
                                                     const int bitwidth,
                                                     const std::string &bytes) {
  IrValue result;
  std::string normalized_bytes;
  if (format != Format::STRING) {
    ASSIGN_OR_RETURN(normalized_bytes,
                     ArbitraryToNormalizedByteString(bytes, bitwidth));
  }
  switch (format) {
    case Format::MAC: {
      ASSIGN_OR_RETURN(auto mac, NormalizedByteStringToMac(normalized_bytes));
      result.set_mac(mac);
      break;
    }
    case Format::IPV4: {
      ASSIGN_OR_RETURN(auto ipv4, NormalizedByteStringToIpv4(normalized_bytes));
      result.set_ipv4(ipv4);
      break;
    }
    case Format::IPV6: {
      ASSIGN_OR_RETURN(auto ipv6, NormalizedByteStringToIpv6(normalized_bytes));
      result.set_ipv6(ipv6);
      break;
    }
    case Format::STRING: {
      result.set_str(bytes);
      break;
    }
    case Format::HEX_STRING: {
      auto hex_string = absl::BytesToHexString(
          NormalizedToCanonicalByteString(normalized_bytes));
      hex_string.erase(0, std::min(hex_string.find_first_not_of('0'),
                                   hex_string.size() - 1));
      result.set_hex_str(absl::StrCat("0x", hex_string));
      break;
    }
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unexpected format: " << Format_Name(format);
  }
  return result;
}

absl::Status ValidateIrValueFormat(const IrValue &ir_value,
                                   const Format &format) {
  const auto &format_case = ir_value.format_case();
  ASSIGN_OR_RETURN(const std::string format_case_name,
                   gutil::GetOneOfFieldName(ir_value, std::string("format")));
  switch (format) {
    case Format::MAC: {
      if (format_case != IrValue::kMac) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format \"" << Format_Name(Format::MAC)
               << "\", but got \"" << format_case_name << "\" instead.";
      }
      break;
    }
    case Format::IPV4: {
      if (format_case != IrValue::kIpv4) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format \"" << Format_Name(Format::IPV4)
               << "\", but got \"" << format_case_name << "\" instead.";
      }
      break;
    }
    case Format::IPV6: {
      if (format_case != IrValue::kIpv6) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format \"" << Format_Name(Format::IPV6)
               << "\", but got \"" << format_case_name << "\" instead.";
      }
      break;
    }
    case Format::STRING: {
      if (format_case != IrValue::kStr) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format \"" << Format_Name(Format::STRING)
               << "\", but got \"" << format_case_name << "\" instead.";
      }
      break;
    }
    case Format::HEX_STRING: {
      if (format_case != IrValue::kHexStr) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format \"" << Format_Name(Format::HEX_STRING)
               << "\", but got \"" << format_case_name << "\" instead.";
      }
      std::string hex_str = ir_value.hex_str();
      if (absl::StartsWith(hex_str, "0x")) {
        hex_str.replace(0, 2, "");
      }
      break;
    }
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unexpected format: " << Format_Name(format);
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> IrValueToNormalizedByteString(
    const IrValue &ir_value, const int bitwidth) {
  std::string byte_string;
  const auto &format_case = ir_value.format_case();
  ASSIGN_OR_RETURN(const std::string format_case_name,
                   gutil::GetOneOfFieldName(ir_value, std::string("format")));
  switch (format_case) {
    case IrValue::kMac: {
      ASSIGN_OR_RETURN(byte_string, MacToNormalizedByteString(ir_value.mac()));
      break;
    }
    case IrValue::kIpv4: {
      ASSIGN_OR_RETURN(byte_string,
                       Ipv4ToNormalizedByteString(ir_value.ipv4()));
      break;
    }
    case IrValue::kIpv6: {
      ASSIGN_OR_RETURN(byte_string,
                       Ipv6ToNormalizedByteString(ir_value.ipv6()));
      break;
    }
    case IrValue::kStr: {
      byte_string = ir_value.str();
      break;
    }
    case IrValue::kHexStr: {
      const std::string &hex_str = ir_value.hex_str();
      if (!absl::StartsWith(hex_str, "0x")) {
        return gutil::InvalidArgumentErrorBuilder()
               << "IR Value \"" << hex_str
               << "\" with hex string format does not start with 0x.";
      }
      absl::string_view stripped_hex = absl::StripPrefix(hex_str, "0x");
      if (!std::all_of(stripped_hex.begin(), stripped_hex.end(),
                       [](const char c) {
                         return std::isxdigit(c) != 0 && c == std::tolower(c);
                       })) {
        return gutil::InvalidArgumentErrorBuilder()
               << "IR Value \"" << hex_str
               << "\" contains non-hexadecimal characters";
      }

      if (stripped_hex.size() % 2) {
        byte_string = absl::HexStringToBytes(absl::StrCat("0", stripped_hex));
      } else {
        byte_string = absl::HexStringToBytes(stripped_hex);
      }
      break;
    }
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unexpected format: " << format_case_name;
  }

  std::string result = byte_string;
  if (format_case != IrValue::kStr) {
    ASSIGN_OR_RETURN(result,
                     ArbitraryToNormalizedByteString(byte_string, bitwidth));
  }
  return result;
}

absl::StatusOr<IrValue> FormattedStringToIrValue(const std::string &value,
                                                 Format format) {
  IrValue result;
  switch (format) {
    case Format::MAC:
      result.set_mac(value);
      break;
    case Format::IPV4:
      result.set_ipv4(value);
      break;
    case Format::IPV6:
      result.set_ipv6(value);
      break;
    case Format::STRING:
      result.set_str(value);
      break;
    case Format::HEX_STRING:
      result.set_hex_str(value);
      break;
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unexpected format: " << Format_Name(format);
  }
  return result;
}

absl::StatusOr<std::string> IrValueToFormattedString(const IrValue &value,
                                                     Format format) {
  switch (format) {
    case Format::MAC:
      return value.mac();
    case Format::IPV4:
      return value.ipv4();
    case Format::IPV6:
      return value.ipv6();
    case Format::STRING:
      return value.str();
    case Format::HEX_STRING:
      return value.hex_str();
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unexpected format: " << Format_Name(format);
  }
}

bool IsAllZeros(const std::string &s) {
  bool has_non_zero_value = false;
  for (const auto &c : s) {
    if (c != '\x00') {
      has_non_zero_value = true;
      break;
    }
  }

  return has_non_zero_value == false;
}

absl::StatusOr<std::string> Intersection(const std::string &left,
                                         const std::string &right) {
  if (left.size() != right.size()) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Cannot find intersection. \"" << absl::CEscape(left) << "\"("
           << left.size() << " bytes) and \"" << absl::CEscape(right) << "\"("
           << right.size() << " bytes) are of unequal length.";
  }
  std::string result = "";
  for (int i = 0; i < left.size(); ++i) {
    char left_c = left[i];
    char right_c = right[i];
    result += (left_c & right_c);
  }
  return result;
}

absl::StatusOr<std::string> PrefixLenToMask(int prefix_len, int bitwidth) {
  if (prefix_len > bitwidth) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Prefix length " << prefix_len
           << " cannot be greater than bitwidth " << bitwidth;
  }

  std::string result;
  if (bitwidth % 8) {
    int msb = bitwidth % 8;
    result += (0xff >> (kNumBitsInByte - msb) & 0xff);
    prefix_len -= msb;
    bitwidth -= msb;
  }
  for (int i = bitwidth; i > 0; i -= kNumBitsInByte) {
    if (prefix_len >= (int)kNumBitsInByte) {
      result += '\xff';
    } else {
      if (prefix_len > 0) {
        result += (0xff << (kNumBitsInByte - prefix_len) & 0xff);
      } else {
        result += '\x00';
      }
    }
    prefix_len -= kNumBitsInByte;
  }
  return result;
}

bool RequiresPriority(const IrTableDefinition &ir_table_definition) {
  const auto &matches = ir_table_definition.match_fields_by_name();
  for (auto it = matches.begin(); it != matches.end(); it++) {
    switch (it->second.match_field().match_type()) {
      case p4::config::v1::MatchField::OPTIONAL:
      case p4::config::v1::MatchField::RANGE:
      case p4::config::v1::MatchField::TERNARY:
        return true;
      default:
        break;
    }
  }
  return false;
}
absl::Status IsGoogleRpcCode(int rpc_code) {
  if (rpc_code < 0 || rpc_code > 15) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Invalid status code: " << rpc_code;
  }
  return absl::OkStatus();
}

absl::Status ValidateGenericUpdateStatus(google::rpc::Code code,
                                         const std::string &message) {
  if (code == google::rpc::OK && !message.empty()) {
    return absl::InvalidArgumentError(
        "OK status should not contain error message.");
  }
  if (code != google::rpc::OK && message.empty()) {
    return absl::InvalidArgumentError(
        "UpdateStatus with non-ok status must have error message.");
  }
  return absl::OkStatus();
}

std::string IrWriteResponseToReadableMessage(
    const IrWriteResponse &ir_write_response) {
  std::string readable_message;
  absl::StrAppend(&readable_message, "Batch failed, individual results:\n");
  int i = 1;
  for (const auto &ir_update_status : ir_write_response.statuses()) {
    absl::StrAppend(&readable_message, "#", i, ": ",
                    absl::StatusCodeToString(static_cast<absl::StatusCode>(
                        ir_update_status.code())));
    if (!ir_update_status.message().empty()) {
      absl::StrAppend(&readable_message, ": ", ir_update_status.message(),
                      "\n");
    } else {
      // Insert a new line for OK status.
      absl::StrAppend(&readable_message, "\n");
    }
    ++i;
  }

  return readable_message;
}
}  // namespace pdpi
