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

#ifndef GUTIL_TESTING_H
#define GUTIL_TESTING_H

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <string>

#include "google/protobuf/util/message_differencer.h"
#include "gutil/status.h"

namespace gutil {

// ASSERT_OK_AND_ASSIGN evaluates the expression (which needs to evaluate to a
// StatusOr) and asserts that the expression has status OK. It then assigns the
// result to lhs, and otherwise fails.
#define ASSERT_OK_AND_ASSIGN(lhs, expression)                           \
  auto __ASSIGN_OR_RETURN_VAL(__LINE__) = expression;                   \
  if (!__ASSIGN_OR_RETURN_VAL(__LINE__).status().ok()) {                \
    FAIL() << #expression                                               \
           << " failed: " << __ASSIGN_OR_RETURN_VAL(__LINE__).status(); \
  }                                                                     \
  lhs = __ASSIGN_OR_RETURN_VAL(__LINE__).value();

// ASSERT_OK checks that the expression has status OK
#define ASSERT_OK(expression)                             \
  {                                                       \
    auto status = expression;                             \
    if (!status.ok())                                     \
      FAIL() << "Expected " << #expression                \
             << " to be OK, but instead got: " << status; \
  };

// Same as ASSERT_OK, but using EXPECT_*.
#define EXPECT_OK(expression)                                    \
  {                                                              \
    auto status = expression;                                    \
    if (!status.ok())                                            \
      ADD_FAILURE() << "Expected " << #expression                \
                    << " to be OK, but instead got: " << status; \
  };

// Crash if `status` is not okay. Only use in tests.
#define CHECK_OK(expr)                                                       \
  {                                                                          \
    auto status = expr;                                                      \
    if (!status.ok()) {                                                      \
      std::cerr << "CHECK_OK(" << #expr                                      \
                << ") failed. Status was:" << status.message() << std::endl; \
      exit(1);                                                               \
    }                                                                        \
  }

// Crash if `expr` is false. Only use in tests.
#define CHECK(expr)                                             \
  if (!(expr)) {                                                \
    std::cerr << "CHECK(" << #expr << ") failed." << std::endl; \
    exit(1);                                                    \
  }

// Parses a protobuf from a string, and crashes if parsing failed. Only use in
// tests.
template <typename T>
T ParseProtoOrDie(const std::string& proto_string) {
  T message;
  CHECK_OK(ReadProtoFromString(proto_string, &message));
  return message;
}

}  // namespace gutil

#endif  // GUTIL_TESTING_H
