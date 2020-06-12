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

#include "p4_pdpi/util.h"

#include <google/protobuf/descriptor.h>

#include <algorithm>

#include "absl/algorithm/container.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/types/optional.h"

namespace pdpi {

using ::pdpi::ir::Format;
using ::pdpi::ir::IrValue;

absl::Status ReadProtoFromFile(const std::string &filename,
                               google::protobuf::Message *message) {
  // Verifies that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    return InvalidArgumentErrorBuilder()
           << "Error opening the file " << filename << ".";
  }

  google::protobuf::io::FileInputStream file_stream(fd);
  file_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_stream, message)) {
    return InvalidArgumentErrorBuilder()
           << "Failed to parse file " << filename << ".";
  }

  return absl::OkStatus();
}

absl::Status ReadProtoFromString(const std::string &proto_string,
                                 google::protobuf::Message *message) {
  // Verifies that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (!google::protobuf::TextFormat::ParseFromString(proto_string, message)) {
    return InvalidArgumentErrorBuilder()
           << "Failed to parse string " << proto_string << ".";
  }

  return absl::OkStatus();
}

StatusOr<std::string> Normalize(const std::string &pi_byte_string,
                                int expected_bitwidth) {
  std::string stripped_value = pi_byte_string;
  // Remove leading zeros
  stripped_value.erase(0, std::min(stripped_value.find_first_not_of('\x00'),
                                   stripped_value.size() - 1));
  int length = GetBitwidthOfPiByteString(stripped_value);
  if (length > expected_bitwidth) {
    return InvalidArgumentErrorBuilder()
           << "Value of length " << length << " is greater than bitwidth "
           << expected_bitwidth;
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

StatusOr<uint64_t> PiByteStringToUint(const std::string &pi_bytes,
                                      int bitwidth) {
  if (bitwidth > 64) {
    return absl::Status(absl::StatusCode::kInternal,
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

std::string PiByteStringToMac(const std::string &normalized_bytes) {
  std::vector<std::string> parts;
  for (const char c : normalized_bytes) {
    parts.push_back(absl::StrCat(absl::Hex(int{c & 0xFF}, absl::kZeroPad2)));
  }
  return absl::StrJoin(parts, ":");
}

std::string PiByteStringToIpv4(const std::string &normalized_bytes) {
  std::vector<std::string> parts;
  for (const char c : normalized_bytes) {
    parts.push_back(absl::StrCat(absl::Hex((int)c)));
  }
  return absl::StrJoin(parts, ".");
}

std::string PiByteStringToIpv6(const std::string &normalized_bytes) {
  // TODO: Find a way to store in shorthand IPv6 notation
  std::vector<std::string> parts;
  for (unsigned int i = 0; i < kNumBytesInIpv6; i += 2) {
    parts.push_back(
        absl::StrCat(absl::Hex((int)normalized_bytes[i], absl::kZeroPad2),
                     absl::Hex((int)normalized_bytes[i + 1], absl::kZeroPad2)));
  }
  return absl::StrJoin(parts, ":");
}

// Based off
// https://github.com/googleapis/gapic-generator-cpp/blob/master/generator/internal/gapic_utils.cc
std::string CamelCaseToSnakeCase(const std::string &input) {
  std::string output;
  for (auto i = 0u; i < input.size(); ++i) {
    if (i + 2 < input.size()) {
      if (std::isupper(input[i + 1]) && std::islower(input[i + 2])) {
        absl::StrAppend(&output, std::string(1, std::tolower(input[i])), "_");
        continue;
      }
    }
    if (i + 1 < input.size()) {
      if ((std::islower(input[i]) || std::isdigit(input[i])) &&
          std::isupper(input[i + 1])) {
        absl::StrAppend(&output, std::string(1, std::tolower(input[i])), "_");
        continue;
      }
    }
    absl::StrAppend(&output, std::string(1, std::tolower(input[i])));
  }
  return output;
}

std::string ProtoFriendlyName(const std::string &p4_name) {
  std::string fieldname = p4_name;
  fieldname.erase(std::remove(fieldname.begin(), fieldname.end(), ']'),
                  fieldname.end());
  absl::c_replace(fieldname, '[', '_');
  absl::c_replace(fieldname, '.', '_');
  return CamelCaseToSnakeCase(fieldname);
}

std::string TableEntryFieldname(const std::string &alias) {
  return absl::StrCat(ProtoFriendlyName(alias), "_entry");
}

std::string ActionFieldname(const std::string &alias) {
  return ProtoFriendlyName(alias);
}

StatusOr<const google::protobuf::FieldDescriptor *> GetFieldDescriptorByName(
    const std::string &fieldname, google::protobuf::Message *parent_message) {
  auto *parent_descriptor = parent_message->GetDescriptor();
  auto *field_descriptor = parent_descriptor->FindFieldByName(fieldname);
  if (field_descriptor == nullptr) {
    return InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message->GetTypeName() << ".";
  }
  return field_descriptor;
}

StatusOr<google::protobuf::Message *> GetMessageByFieldname(
    const std::string &fieldname, google::protobuf::Message *parent_message) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptorByName(fieldname, parent_message));
  if (field_descriptor == nullptr) {
    return InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message->GetTypeName() << ". "
           << kPdProtoAndP4InfoOutOfSync;
  }

  return parent_message->GetReflection()->MutableMessage(parent_message,
                                                         field_descriptor);
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

StatusOr<Format> GetFormat(const std::vector<std::string> &annotations,
                           const int bitwidth,
                           const absl::optional<std::string> &named_type) {
  Format format = Format::HEX_STRING;
  if (named_type.has_value()) {
    std::string type = named_type.value();
    if (type == "router_interface_id_t" || type == "neighbor_id_t" ||
        type == "nexthop_id_t" || type == "wcmp_group_id_t") {
      format = Format::STRING;
    }
  }
  for (const std::string &annotation : annotations) {
    if (absl::StartsWith(annotation, "@format(")) {
      if (format != Format::HEX_STRING) {
        return InvalidArgumentErrorBuilder()
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
    return InvalidArgumentErrorBuilder()
           << "Only 48 bit values can be formatted as a MAC address.";
  }
  if (format == Format::IPV4 && bitwidth != kNumBitsInIpv4) {
    return InvalidArgumentErrorBuilder()
           << "Only 32 bit values can be formatted as an IPv4 address.";
  }
  if (format == Format::IPV6 && bitwidth != kNumBitsInIpv6) {
    return InvalidArgumentErrorBuilder()
           << "Only 128 bit values can be formatted as an IPv6 address.";
  }
  return format;
}

StatusOr<IrValue> FormatByteString(const Format &format, const int bitwidth,
                                   const std::string &pi_value) {
  IrValue result;
  ASSIGN_OR_RETURN(const auto &normalized_bytes, Normalize(pi_value, bitwidth));
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
      ASSIGN_OR_RETURN(uint64_t value, PiByteStringToUint(pi_value, bitwidth));
      result.set_str(absl::StrCat(value));
      break;
    }
    case Format::HEX_STRING: {
      result.set_hex_str(absl::BytesToHexString(normalized_bytes));
      break;
    }
    default:
      return InvalidArgumentErrorBuilder() << "Unexpected format: "
                                           << Format_Name(format);
  }
  return result;
}

std::string EscapeString(const std::string &s) {
  std::string result = absl::CHexEscape(s);
  absl::StrReplaceAll({{"\"", "\\\""}}, &result);
  return result;
}

}  // namespace pdpi
