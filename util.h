#ifndef UTIL_H
#define UTIL_H

#include <fcntl.h>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace pdpi {

const uint32_t kNumBitsInByte = 8;

constexpr char kPdProtoAndP4InfoOutOfSync[] =
  "The PD proto and P4Info file are out of sync.";

class internal_error : public std::runtime_error {
 public:
  internal_error(const std::string &error_msg): std::runtime_error(error_msg) {}
};

// Read the contents of the file into a protobuf
void ReadProtoFromFile(const std::string &filename,
                       google::protobuf::Message *message);

// Read the contents of the string into a protobuf
void ReadProtoFromString(const std::string &proto_string,
                         google::protobuf::Message *message);

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

// Remove any leading zeros from a given string
void RemoveLeadingZeros(std::string *value);

// Returns the number of bits used by the PI byte string interpreted as an
// unsigned integer.
uint32_t GetBitwidthOfPiByteString(const std::string &input_string);

}  // namespace pdpi
#endif  // UTIL_H
