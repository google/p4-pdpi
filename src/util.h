#ifndef PDPI_UTIL_H
#define PDPI_UTIL_H

#include <fcntl.h>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace pdpi {

const uint32_t kNumBitsInByte = 8;
const uint32_t kNumBitsInMac = 48;
const uint32_t kNumBytesInMac = kNumBitsInMac/kNumBitsInByte;
const uint32_t kNumBitsInIpv4 = 32;
const uint32_t kNumBytesInIpv4 = kNumBitsInIpv4/kNumBitsInByte;
const uint32_t kNumBitsInIpv6 = 128;
const uint32_t kNumBytesInIpv6 = kNumBitsInIpv6/kNumBitsInByte;

constexpr char kPdProtoAndP4InfoOutOfSync[] =
  "The PD proto and P4Info file are out of sync.";

class internal_error : public std::runtime_error {
 public:
  internal_error(const std::string &error_msg): std::runtime_error(error_msg) {}
};

enum class Format {
  HEX_STRING = 0,
  MAC = 1,
  IPv4 = 2,
  IPv6 = 3,
  STRING = 4,
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Returns the annotation as a FormatEnum
Format GetFormat(const std::vector<std::string> &annotations,
                 const int bitwidth);

// Converts the PI value to an IR value and returns it
std::string FormatByteString(const Format &format,
                             const int bitwidth,
                             const std::string &pi_value);

// Read the contents of the file into a protobuf
void ReadProtoFromFile(const std::string &filename,
                       google::protobuf::Message *message);

// Read the contents of the string into a protobuf
void ReadProtoFromString(const std::string &proto_string,
                         google::protobuf::Message *message);

// Returns a string of length ceil(expected_bitwidth/8).
// Throws an exception for any byte string that is not well-formed
// (e.g., too many bytes, or empty).
std::string Normalize(const std::string& pi_byte_string,
                      int expected_bitwidth);

// Convert the given byte string into a uint value
uint64_t PiByteStringToUint(const std::string& pi_bytes, int bitwidth);

// Convert the given byte string into a : separated MAC representation
// Input string should have Normalize() called on it before being passed in
std::string PiByteStringToMac(const std::string& normalized_bytes);

// Convert the given byte string into a . separated IPv4 representation
// Input string should have Normalize() called on it before being passed in
std::string PiByteStringToIpv4(const std::string& normalized_bytes);

// Convert the given byte string into a : separated IPv6 representation
// Input string should have Normalize() called on it before being passed in
std::string PiByteStringToIpv6(const std::string& normalized_bytes);

// Modify the p4_name in a way that is acceptable as fields in protobufs
std::string ProtoFriendlyName(const std::string &p4_name);

// Return the name of the field in the PD table entry given the alias of the
// field
std::string TableEntryFieldname(const std::string &alias);

// Return the name of the field of the PD action given the alias of the
// field
std::string ActionFieldname(const std::string &alias);

// Return a mutable message given the name of the message field
google::protobuf::Message *GetMessageByFieldname(
    const std::string &fieldname,
    google::protobuf::Message *parent_message);

// Return the descriptor of the field to be used in the reflection API
const google::protobuf::FieldDescriptor *GetFieldDescriptorByName(
    const std::string &fieldname,
    google::protobuf::Message *parent_message);

// Returns the number of bits used by the PI byte string interpreted as an
// unsigned integer.
uint32_t GetBitwidthOfPiByteString(const std::string &input_string);

// Checks if the id is unique in set. Otherwise throws an invalid_argument
// exception with the given error message.
void InsertIfUnique(absl::flat_hash_set<uint32_t>& set,
                    uint32_t id,
                    const std::string& error_message);

// Checks if the key is unique in map. Otherwise throws an invalid_argument
// exception with the given error message.
template <typename M>
void InsertIfUnique(absl::flat_hash_map<uint32_t, M>& map,
                    uint32_t key,
                    M val,
                    const std::string& error_message) {
  auto it = map.insert({key, val});
  if (!it.second) {
    throw std::invalid_argument(error_message);
  }
}

// Returns map[key] if key exists in map. Otherwise throws an invalid_argument
// exception with the given error message.
template <typename M>
M FindElement(const absl::flat_hash_map<uint32_t, M>& map,
              uint32_t key,
              const std::string& error_message) {
  auto it = map.find(key);
  if (it == map.end()) {
    throw std::invalid_argument(error_message);
  }
  return it->second;
}

// Returns an escaped version of s, escaping non-printable characters and
// double quotes.
std::string EscapeString(const std::string& s);

}  // namespace pdpi
#endif  // PDPI_UTIL_H
