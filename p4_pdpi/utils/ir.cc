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
#include <netinet/ether.h>

#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "gutil/proto.h"

namespace pdpi {

using ::p4::config::v1::P4NewTypeTranslation;
using ::pdpi::Format;
using ::pdpi::IrValue;

gutil::StatusOr<std::string> Normalize(const std::string &pi_byte_string,
                                       int expected_bitwidth) {
  std::string stripped_value = pi_byte_string;
  // Remove leading zeros
  stripped_value.erase(0, std::min(stripped_value.find_first_not_of('\x00'),
                                   stripped_value.size() - 1));
  int length = GetBitwidthOfPiByteString(stripped_value);
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

gutil::StatusOr<uint64_t> PiByteStringToUint(const std::string &pi_bytes,
                                             int bitwidth) {
  if (bitwidth > 64) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        absl::StrCat("Cannot convert value with "
                                     "bitwidth ",
                                     bitwidth, " to uint."));
  }
  ASSIGN_OR_RETURN(const auto &stripped_value, Normalize(pi_bytes, bitwidth));
  uint64_t nb_value;  // network byte order
  char value[sizeof(nb_value)];
  int pad = sizeof(nb_value) - stripped_value.size();
  if (pad) {
    memset(value, 0, pad);
  }
  memcpy(value + pad, stripped_value.data(), stripped_value.size());
  memcpy(&nb_value, value, sizeof(nb_value));

  return be64toh(nb_value);
}

gutil::StatusOr<std::string> UintToPiByteString(uint64_t value, int bitwidth) {
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

  // TODO(kishanps) remove the leading zeros in the returned string.
  return bytes;
}

std::string PiByteStringToMac(const std::string &normalized_bytes) {
  std::vector<std::string> parts;
  for (const char c : normalized_bytes) {
    parts.push_back(absl::StrCat(absl::Hex(int{c & 0xFF}, absl::kZeroPad2)));
  }
  return absl::StrJoin(parts, ":");
}

gutil::StatusOr<std::string> MacToPiByteString(const std::string &mac) {
  struct ether_addr *byte_string = ether_aton(mac.c_str());
  if (byte_string == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "String cannot be parsed as MAC address:" << mac;
  }
  return std::string((const char *)byte_string->ether_addr_octet);
}

std::string PiByteStringToIpv4(const std::string &normalized_bytes) {
  std::vector<std::string> parts;
  for (const char c : normalized_bytes) {
    parts.push_back(absl::StrCat((int)c));
  }
  return absl::StrJoin(parts, ".");
}

gutil::StatusOr<std::string> Ipv4ToPiByteString(const std::string &ipv4) {
  char ip_addr[4];
  if (inet_pton(AF_INET, ipv4.c_str(), &ip_addr) == 0) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Invalid IPv4 address: " << ipv4;
  }

  return std::string(ip_addr);
}

std::string PiByteStringToIpv6(const std::string &normalized_bytes) {
  // TODO: Find a way to store in shorthand IPv6 notation
  std::vector<std::string> parts;
  for (unsigned int i = 0; i < kNumBytesInIpv6; i += 2) {
    parts.push_back(absl::StrCat(
        absl::Hex(int{normalized_bytes[i] & 0xFF}, absl::kZeroPad2),
        absl::Hex(int{normalized_bytes[i + 1] & 0xFF}, absl::kZeroPad2)));
  }
  return absl::StrJoin(parts, ":");
}

gutil::StatusOr<std::string> Ipv6ToPiByteString(const std::string &ipv6) {
  char ip6_addr[16];
  if (inet_pton(AF_INET6, ipv6.c_str(), &ip6_addr) == 0) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Invalid IPv6 address: " << ipv6;
  }
  return std::string(ip6_addr);
}

uint32_t GetBitwidthOfPiByteString(const std::string &input_string) {
  // Use str.length() - 1. MSB will need to be handled separately since it
  // can have leading zeros which should not be counted.
  int length_in_bits = (input_string.length() - 1) * kNumBitsInByte;

  uint8_t msb = input_string[0];
  while (msb) {
    ++length_in_bits;
    msb >>= 1;
  }

  return length_in_bits;
}

gutil::StatusOr<Format> GetFormat(const std::vector<std::string> &annotations,
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
      }
      if (annotation == "@format(IPV4_ADDRESS)") {
        format = Format::IPV4;
      }
      if (annotation == "@format(IPV6_ADDRESS)") {
        format = Format::IPV6;
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

gutil::StatusOr<IrValue> FormatByteString(const Format &format,
                                          const int bitwidth,
                                          const std::string &pi_value) {
  IrValue result;
  std::string normalized_bytes;
  if (format != Format::STRING) {
    ASSIGN_OR_RETURN(normalized_bytes, Normalize(pi_value, bitwidth));
  }
  switch (format) {
    case Format::MAC: {
      result.set_mac(PiByteStringToMac(normalized_bytes));
      break;
    }
    case Format::IPV4: {
      result.set_ipv4(PiByteStringToIpv4(normalized_bytes));
      break;
    }
    case Format::IPV6: {
      result.set_ipv6(PiByteStringToIpv6(normalized_bytes));
      break;
    }
    case Format::STRING: {
      result.set_str(pi_value);
      break;
    }
    case Format::HEX_STRING: {
      result.set_hex_str(
          absl::StrCat("0x", absl::BytesToHexString(normalized_bytes)));
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
               << "Expected format " << Format_Name(Format::MAC) << ", but got "
               << format_case_name << " instead.";
      }
      break;
    }
    case Format::IPV4: {
      if (format_case != IrValue::kIpv4) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format " << Format_Name(Format::IPV4)
               << ", but got " << format_case_name << " instead.";
      }
      break;
    }
    case Format::IPV6: {
      if (format_case != IrValue::kIpv6) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format " << Format_Name(Format::IPV6)
               << ", but got " << format_case_name << " instead.";
      }
      break;
    }
    case Format::STRING: {
      if (format_case != IrValue::kStr) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format " << Format_Name(Format::STRING)
               << ", but got " << format_case_name << " instead.";
      }
      break;
    }
    case Format::HEX_STRING: {
      if (format_case != IrValue::kHexStr) {
        return gutil::InvalidArgumentErrorBuilder()
               << "Expected format " << Format_Name(Format::HEX_STRING)
               << ", but got " << format_case_name << " instead.";
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

gutil::StatusOr<std::string> IrValueToByteString(const IrValue &ir_value) {
  std::string byte_string;
  const auto &format_case = ir_value.format_case();
  ASSIGN_OR_RETURN(const std::string format_case_name,
                   gutil::GetOneOfFieldName(ir_value, std::string("format")));
  switch (format_case) {
    case IrValue::kMac: {
      ASSIGN_OR_RETURN(byte_string, MacToPiByteString(ir_value.mac()));
      break;
    }
    case IrValue::kIpv4: {
      ASSIGN_OR_RETURN(byte_string, Ipv4ToPiByteString(ir_value.ipv4()));
      break;
    }
    case IrValue::kIpv6: {
      ASSIGN_OR_RETURN(byte_string, Ipv6ToPiByteString(ir_value.ipv6()));
      break;
    }
    case IrValue::kStr: {
      byte_string = ir_value.str();
      break;
    }
    case IrValue::kHexStr: {
      std::string hex_str = ir_value.hex_str();
      if (!absl::StartsWith(hex_str, "0x")) {
        return gutil::InvalidArgumentErrorBuilder()
               << "IR Value " << hex_str
               << " with hex string format does not start with 0x.";
      }
      absl::string_view stripped_hex = absl::StripPrefix(hex_str, "0x");
      if (!std::all_of(stripped_hex.begin(), stripped_hex.end(),
                       [](const char c) {
                         return std::isxdigit(c) and c == std::tolower(c);
                       })) {
        return gutil::InvalidArgumentErrorBuilder()
               << "IR Value " << hex_str
               << " contains non-hexadecimal characters";
      }

      byte_string = absl::HexStringToBytes(stripped_hex);
      break;
    }
    default:
      return gutil::InvalidArgumentErrorBuilder()
             << "Unexpected format: " << format_case_name;
  }

  return byte_string;
}

gutil::StatusOr<IrValue> FormattedStringToIrValue(const std::string &value,
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
}  // namespace pdpi
