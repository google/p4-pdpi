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

#include "p4_pdpi/utils/annotation_parser.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "gutil/status.h"
#include "re2/re2.h"

namespace pdpi {
namespace annotation {

namespace internal {
absl::StatusOr<AnnotationComponents> ParseAnnotation(
    const std::string& annotation) {
  // Regex: @<label>
  static constexpr LazyRE2 kLabelOnlyParser = {R"([ \t]*@([^ \t(]*)[ \t]*)"};
  // Regex: @<label> *(<&body>)
  static constexpr LazyRE2 kParser = {
      R"([ \t]*@([^ \t(]*)[ \t]*\((.*)\)[ \t]*)"};
  std::string label, body;

  if (RE2::FullMatch(annotation, *kLabelOnlyParser, &label)) {
    return AnnotationComponents({.label = std::move(label)});
  }
  if (RE2::FullMatch(annotation, *kParser, &label, &body)) {
    return AnnotationComponents(
        {.label = std::move(label), .body = std::move(body)});
  }
  return gutil::InvalidArgumentErrorBuilder()
         << "Annotation \"" << annotation << "\" is malformed.";
}
}  // namespace internal

// Parses an annotation value and returns the component arguments in order.
// Arguments are comma-delimited. Returned arguments are stripped of whitespace.
absl::StatusOr<std::vector<std::string>> ParseAsArgList(std::string value) {
  // Limit arg characters to alphanumeric, underscore, whitespace, and forward
  // slash.
  static constexpr LazyRE2 kSanitizer = {R"([a-zA-Z0-9_/, \t]*)"};

  if (!RE2::FullMatch(value, *kSanitizer)) {
    return gutil::InvalidArgumentErrorBuilder()
           << "Argument string contains invalid characters for argument list "
           << "parsing. Valid characters: [a-zA-Z0-9_/, \t].";
  }

  std::string no_space_arg =
      absl::StrReplaceAll(value, {{" ", ""}, {"\t", ""}});
  if (no_space_arg.empty()) {
    return std::vector<std::string>();
  }
  std::vector<std::string> arg_list = absl::StrSplit(no_space_arg, ',');
  return arg_list;
}

}  // namespace annotation
}  // namespace pdpi
