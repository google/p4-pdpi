#include "util.h"
#include "absl/strings/str_cat.h"

void ReadProtoFromFile(const std::string &filename,
                       google::protobuf::Message *message) {
  // Verifies that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  int fd = open(filename.c_str(), O_RDONLY);
  if(fd < 0) {
    throw std::invalid_argument(absl::StrCat("Error opening the file ",
                                            filename));
  }

  google::protobuf::io::FileInputStream file_stream(fd);
  file_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_stream, message)) {
    throw std::invalid_argument(absl::StrCat("Failed to parse file!"));
  }
}

void ReadProtoFromString(const std::string &proto_string,
                         google::protobuf::Message *message) {
  // Verifies that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (!google::protobuf::TextFormat::ParseFromString(proto_string, message)) {
    throw std::invalid_argument(absl::StrCat("Failed to parse string!"));
  }
}
