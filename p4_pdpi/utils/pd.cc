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
