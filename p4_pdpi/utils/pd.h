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

#ifndef P4_PDPI_UTILS_PD_H
#define P4_PDPI_UTILS_PD_H

#include <google/protobuf/message.h>

#include "gutil/status.h"

namespace pdpi {

constexpr char kPdProtoAndP4InfoOutOfSync[] =
    "The PD proto and P4Info file are out of sync.";

// Given a P4 name, returns the name of the corresponding protobuf message name.
gutil::StatusOr<std::string> P4NameToProtobufMessageName(
    const std::string &p4_name);

// Given a P4 name, returns the name of the corresponding protobuf field name.
gutil::StatusOr<std::string> P4NameToProtobufFieldName(
    const std::string &p4_name);

// Return a mutable message given the name of the message field.
gutil::StatusOr<google::protobuf::Message *> GetMessageByFieldname(
    const std::string &fieldname, google::protobuf::Message *parent_message);

// Return the descriptor of the field to be used in the reflection API.
gutil::StatusOr<const google::protobuf::FieldDescriptor *>
GetFieldDescriptorByName(const std::string &fieldname,
                         google::protobuf::Message *parent_message);

}  // namespace pdpi
#endif  // P4_PDPI_UTILS_PD_H
