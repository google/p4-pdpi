#include "src/util.h"

#include <algorithm>

#include "absl/algorithm/container.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include <google/protobuf/descriptor.h>

namespace pdpi {
void ReadProtoFromFile(const std::string &filename,
                       google::protobuf::Message *message) {
  // Verifies that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  int fd = open(filename.c_str(), O_RDONLY);
  if(fd < 0) {
    throw std::invalid_argument(absl::StrCat("Error opening the file ",
                                            filename, "."));
  }

  google::protobuf::io::FileInputStream file_stream(fd);
  file_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_stream, message)) {
    throw std::invalid_argument(absl::StrCat("Failed to parse file ", filename,
                                             "."));
  }
}

void ReadProtoFromString(const std::string &proto_string,
                         google::protobuf::Message *message) {
  // Verifies that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (!google::protobuf::TextFormat::ParseFromString(proto_string, message)) {
    throw std::invalid_argument(absl::StrCat("Failed to parse string ",
                                             proto_string, "."));
  }
}

std::string Normalize(const std::string& pi_byte_string,
                      int expected_bitwidth) {
  std::string stripped_value = pi_byte_string;
  // Remove leading zeros
  stripped_value.erase(0, std::min(stripped_value.find_first_not_of('\x00'),
                                   stripped_value.size()-1));
  int length = GetBitwidthOfPiByteString(stripped_value);
  if (length > expected_bitwidth) {
    throw std::invalid_argument(absl::StrCat("Value of length ", length,
                                             " is greater than bitwidth ",
                                             expected_bitwidth));
  }

  int total_bytes;
  if (expected_bitwidth % 8) {
    total_bytes = expected_bitwidth/8 + 1;
  } else {
    total_bytes = expected_bitwidth/8;
  }
  std::string zeros = std::string(total_bytes - stripped_value.length(),
                                  '\x00');
  return zeros.append(stripped_value);
}

// Checks that pi_bytes fits into bitwdith many bits, and returns the value
// as a uint64_t.
uint64_t PiByteStringToUint(const std::string& pi_bytes, int bitwidth) {
  if (bitwidth > 64) {
    throw internal_error(absl::StrCat("Cannot convert value with "
                                      "bitwidth ", bitwidth,
                                      " to uint."));
  }
  std::string stripped_value = Normalize(pi_bytes, bitwidth);
  uint64_t nb_value; // network byte order
  char value[sizeof(nb_value)];
  int pad = sizeof(nb_value) - stripped_value.size();
  if (pad) {
    memset(value, 0, pad);
  }
  memcpy(value + pad, stripped_value.data(), stripped_value.size());
  memcpy(&nb_value, value, sizeof(nb_value));

  return be64toh(nb_value);
}

// Checks that pi_bytes fits into bitwdith many bits, and returns the value
// as a formatted MAC string.
std::string PiByteStringToMac(const std::string& normalized_bytes) {
  std::vector<std::string> parts;
  for (const char c : normalized_bytes) {
    parts.push_back(absl::StrCat(absl::Hex((int)c, absl::kZeroPad2)));
  }
  return absl::StrJoin(parts, ":");
}

// Checks that pi_bytes fits into bitwdith many bits, and returns the value
// as a formatted IPv4 string.
std::string PiByteStringToIpv4(const std::string& normalized_bytes) {
  std::vector<std::string> parts;
  for (const char c : normalized_bytes) {
    parts.push_back(absl::StrCat(absl::Hex((int)c)));
  }
  return absl::StrJoin(parts, ".");
}

// Checks that pi_bytes fits into bitwdith many bits, and returns the value
// as a formatted IPv6 string.
std::string PiByteStringToIpv6(const std::string& normalized_bytes) {
  // TODO: Find a way to store in shorthand IPv6 notation
  std::vector<std::string> parts;
  for (unsigned int i = 0; i < kNumBytesInIpv6; i += 2) {
    parts.push_back(absl::StrCat(absl::Hex((int)normalized_bytes[i],
                                           absl::kZeroPad2),
                                 absl::Hex((int)normalized_bytes[i+1],
                                           absl::kZeroPad2)));
  }
  return absl::StrJoin(parts, ":");
}

// Based off
// https://github.com/googleapis/gapic-generator-cpp/blob/master/generator/internal/gapic_utils.cc
std::string CamelCaseToSnakeCase(const std::string& input) {
  std::string output;
  for (auto i = 0u; i < input.size(); ++i) {
    if (i + 2 < input.size()) {
      if (std::isupper(input[i + 1]) &&
          std::islower(input[i + 2])) {
        absl::StrAppend(&output,
                        std::string(1, std::tolower(input[i])), "_");
        continue;
      }
    }
    if (i + 1 < input.size()) {
      if ((std::islower(input[i]) || std::isdigit(input[i])) &&
          std::isupper(input[i + 1])) {
        absl::StrAppend(&output,
                        std::string(1, std::tolower(input[i])), "_");
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

const google::protobuf::FieldDescriptor *GetFieldDescriptorByName(
    const std::string &fieldname,
    google::protobuf::Message *parent_message) {
  auto *parent_descriptor = parent_message->GetDescriptor();
  auto *field_descriptor = parent_descriptor->FindFieldByName(fieldname);
  if (field_descriptor == nullptr) {
    throw std::invalid_argument(absl::StrCat("Field ",
                                             fieldname,
                                             " missing in ",
                                             parent_message->GetTypeName(),
                                             "."));
  }
  return field_descriptor;
}

google::protobuf::Message *GetMessageByFieldname(
    const std::string &fieldname,
    google::protobuf::Message *parent_message) {
  auto *field_descriptor = GetFieldDescriptorByName(fieldname, parent_message);
  if (field_descriptor == nullptr) {
    throw std::invalid_argument(absl::StrCat("Field ",
                                             fieldname,
                                             " missing in ",
                                             parent_message->GetTypeName(),
                                             ". ", kPdProtoAndP4InfoOutOfSync));
  }

  return parent_message->GetReflection()->MutableMessage(parent_message,
                                                         field_descriptor);
}

uint32_t GetBitwidthOfPiByteString(const std::string &input_string) {
  // Use str.length() - 1. MSB will need to be handled separately since it
  // can have leading zeros which should not be counted.
  int length_in_bits = (input_string.length()-1) * kNumBitsInByte;

  uint8_t msb = input_string[0];
  while (msb) {
    ++length_in_bits;
    msb >>= 1;
  }

  return length_in_bits;
}

// Returns the annotation as an enum if it is a supported format annotation
Format GetFormat (const std::vector<std::string> &annotations,
                  const int bitwidth) {
  Format format = Format::HEX_STRING;
  for (const auto &annotation : annotations) {
    if (annotation == "@MAC") {
      format = Format::MAC;
      if (bitwidth != kNumBitsInMac) {
        throw std::invalid_argument(absl::StrCat("Expected bitwidth of ",
                                                 kNumBitsInMac,
                                                 " for field annotated as MAC",
                                                 " but got ", bitwidth,
                                                 " instead."));
      }
    } else if (annotation == "@IPV4_ADDRESS") {
      format = Format::IPv4;
      if (bitwidth != kNumBitsInIpv4) {
        throw std::invalid_argument(absl::StrCat("Expected bitwidth of ",
                                                 kNumBitsInIpv4,
                                                 " for field annotated as IPv4",
                                                 " but got ", bitwidth,
                                                 " instead."));
      }
    } else if (annotation == "@IPV6_ADDRESS") {
      format = Format::IPv6;
      if (bitwidth != kNumBitsInIpv6) {
        throw std::invalid_argument(absl::StrCat("Expected bitwidth of ",
                                                 kNumBitsInIpv6,
                                                 " for field annotated as IPv6",
                                                 " but got ", bitwidth,
                                                 " instead."));
      }
    }
  }
  return format;
}

std::string FormatByteString(const Format &format,
                             const int bitwidth,
                             const std::string &pi_value) {
  std::string normalized_bytes = Normalize(pi_value, bitwidth);
  switch(format) {
    case Format::MAC:
      return PiByteStringToMac(normalized_bytes);
    case Format::IPv4:
      return PiByteStringToIpv4(normalized_bytes);
    case Format::IPv6:
      return PiByteStringToIpv6(normalized_bytes);
    default:
      break;
  }
  return absl::BytesToHexString(normalized_bytes);
}

void InsertIfUnique(absl::flat_hash_set<uint32_t>& set,
                    uint32_t id,
                    const std::string& error_message) {
  const auto it = set.insert(id);
  if (!it.second) {
    throw std::invalid_argument(error_message);
  }
}

std::string EscapeString(const std::string& s) {

  std::string result = absl::CHexEscape(s);
  absl::StrReplaceAll({{"\"", "\\\""}}, &result);
  return result;
}

}  // namespace pdpi
