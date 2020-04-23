#include "src/util.h"

#include <algorithm>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/algorithm/container.h"
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

// Checks that pi_bytes fits into bitwdith many bits, and returns the value
// as a uint64_t.
uint64_t PiByteStringToUint(const std::string& pi_bytes, int bitwidth) {
  if (bitwidth > 64) {
    throw internal_error(absl::StrCat("Cannot convert value with "
                                      "bitwidth ", bitwidth,
                                      " to uint."));
  }
  std::string stripped_value = pi_bytes;
  RemoveLeadingZeros(&stripped_value);
  if (stripped_value.length() > 8) {
    throw std::invalid_argument(absl::StrCat("Cannot convert value longer ",
                                             "than 8 bytes to uint. ",
                                             "Length of ", stripped_value,
                                             " is ", stripped_value.length(),
                                             "."));
  }
  uint64_t nb_value; // network byte order
  char value[sizeof(nb_value)];
  int pad = sizeof(nb_value) - stripped_value.size();
  if (pad) {
    memset(value, 0, pad);
  }
  memcpy(value + pad, stripped_value.data(), stripped_value.size());
  memcpy(&nb_value, value, sizeof(nb_value));

  uint64_t pd_value = be64toh(nb_value);

  int bits_needed = 0;
  uint64_t pd = pd_value;
  while (pd > 0) {
    pd >>= 1;
    ++bits_needed;
  }
  if (bits_needed > bitwidth) {
    throw std::invalid_argument(absl::StrCat("PI value uses ", bits_needed,
                                             " bits and does not fit into ",
                                             bitwidth, " bits."));
  }
  return pd_value;
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
                                             " not found in ",
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
                                             " not found in ",
                                             parent_message->GetTypeName(),
                                             ". ", kPdProtoAndP4InfoOutOfSync));
  }

  return parent_message->GetReflection()->MutableMessage(parent_message,
                                                         field_descriptor);
}

void RemoveLeadingZeros(std::string *value) {
  value->erase(0, std::min(value->find_first_not_of('\x00'), value->size()-1));
}

// Returns the number of bits used by the PI byte string interpreted as an
// unsigned integer.
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

}  // namespace pdpi
