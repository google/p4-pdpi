#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <fcntl.h>
#include <fstream>
#include <google/protobuf/message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

void ReadProtoFromFile(const std::string &filename,
                       google::protobuf::Message *message);

#endif  // UTIL_H
