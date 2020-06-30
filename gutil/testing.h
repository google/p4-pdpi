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

#ifndef PDPI_UTILS_TESTING_H
#define PDPI_UTILS_TESTING_H

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <string>

#include "google/protobuf/util/message_differencer.h"

namespace gutil {

// ASSERT_OK_AND_ASSIGN evaluates the expression (which needs to evaluate to a
// StatusOr) and asserts that the expression has status OK. It then assigns the
// result to lhs, and otherwise fails.
#define ASSERT_OK_AND_ASSIGN(lhs, expression)                   \
  auto evaluated = expression;                                  \
  if (!evaluated.status().ok()) {                               \
    FAIL() << #expression << " failed: " << evaluated.status(); \
  }                                                             \
  lhs = evaluated.value();

// ASSERT_OK checks that the expression has status OK
#define ASSERT_OK(expression)            \
  auto status = expression;              \
  if (!status.ok())                      \
    FAIL() << "Expected " << #expression \
           << " to be OK, but instead got: " << status;

// Same as ASSERT_OK, but using EXPECT_*.
#define EXPECT_OK(expression)                   \
  auto status = expression;                     \
  if (!status.ok())                             \
    ADD_FAILURE() << "Expected " << #expression \
                  << " to be OK, but instead got: " << status;

}  // namespace gutil

#endif  // PDPI_UTILS_TESTING_H
