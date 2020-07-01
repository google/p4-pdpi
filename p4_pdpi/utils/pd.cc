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

#include "p4_pdpi/utils/pd.h"

#include "absl/algorithm/container.h"
#include "gutil/proto.h"

namespace pdpi {

namespace {

// Converts snake_case to PascalCase.
std::string SnakeCaseToPascalCase(const std::string &input) {
  std::string output;
  for (unsigned i = 0; i < input.size(); i += 1) {
    if (input[i] == '_') {
      i += 1;
      if (i < input.size()) {
        absl::StrAppend(&output, std::string(1, std::toupper(input[i])));
      }
    } else if (i == 0) {
      absl::StrAppend(&output, std::string(1, std::toupper(input[i])));
    } else {
      absl::StrAppend(&output, std::string(1, input[i]));
    }
  }
  return output;
}

}  // namespace

gutil::StatusOr<std::string> P4NameToProtobufMessageName(
    const std::string &p4_name) {
  // TODO(heule): validate the name.
  return SnakeCaseToPascalCase(p4_name);
}

gutil::StatusOr<std::string> P4NameToProtobufFieldName(
    const std::string &p4_name) {
  // TODO(heule): validate the name.
  return p4_name;
}

gutil::StatusOr<const google::protobuf::FieldDescriptor *>
GetFieldDescriptorByName(const std::string &fieldname,
                         google::protobuf::Message *parent_message) {
  auto *parent_descriptor = parent_message->GetDescriptor();
  auto *field_descriptor = parent_descriptor->FindFieldByName(fieldname);
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message->GetTypeName() << ".";
  }
  return field_descriptor;
}

gutil::StatusOr<google::protobuf::Message *> GetMessageByFieldname(
    const std::string &fieldname, google::protobuf::Message *parent_message) {
  ASSIGN_OR_RETURN(auto *field_descriptor,
                   GetFieldDescriptorByName(fieldname, parent_message));
  if (field_descriptor == nullptr) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Field " << fieldname << " missing in "
           << parent_message->GetTypeName() << ". "
           << kPdProtoAndP4InfoOutOfSync;
  }

  return parent_message->GetReflection()->MutableMessage(parent_message,
                                                         field_descriptor);
}

}  // namespace pdpi
