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
#include "status_matchers.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gutil/status.h"
#include "gutil/status_matchers.h"

namespace gutil {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(AbseilStatusMatcher, IsOk) { EXPECT_THAT(absl::Status(), IsOk()); }

TEST(AbseilStatusMatcher, IsNotOk) {
  EXPECT_THAT(absl::UnknownError("unknown error"), Not(IsOk()));
}

TEST(AbseilStatusMatcher, StatusIs) {
  EXPECT_THAT(absl::UnknownError("unknown error"),
              StatusIs(absl::StatusCode::kUnknown));
}

TEST(AbseilStatusMatcher, StatusIsNot) {
  EXPECT_THAT(absl::UnknownError("unknown error"),
              Not(StatusIs(absl::StatusCode::kInvalidArgument)));
}

TEST(AbseilStatusMatcher, StatusIsWithMessage) {
  EXPECT_THAT(absl::UnknownError("unknown error"),
              StatusIs(absl::StatusCode::kUnknown, "unknown error"));
}

TEST(AbseilStatusMatcher, StatusIsWithMessageNot) {
  EXPECT_THAT(absl::UnknownError("unknown error"),
              Not(StatusIs(absl::StatusCode::kInvalidArgument, "unknown")));
}

TEST(GutilStatusOrMatcher, IsOk) {
  EXPECT_THAT(absl::StatusOr<int>(1), IsOk());
}

TEST(GutilStatusOrMatcher, IsNotOk) {
  EXPECT_THAT(absl::StatusOr<int>(absl::UnknownError("unknown error")),
              Not(IsOk()));
}

TEST(GutilStatusOrMatcher, StatusIs) {
  EXPECT_THAT(absl::StatusOr<int>(absl::UnknownError("unknown error")),
              StatusIs(absl::StatusCode::kUnknown));
}

TEST(GutilStatusOrMatcher, StatusIsNot) {
  EXPECT_THAT(absl::StatusOr<int>(absl::UnknownError("unknown error")),
              Not(StatusIs(absl::StatusCode::kInvalidArgument)));
}

TEST(GutilStatusOrMatcher, StatusIsWithMessage) {
  EXPECT_THAT(absl::StatusOr<int>(absl::UnknownError("unknown error")),
              StatusIs(absl::StatusCode::kUnknown, HasSubstr("unknown")));
}

TEST(GutilStatusOrMatcher, StatusIsWithMessageNot) {
  EXPECT_THAT(absl::StatusOr<int>(absl::UnknownError("unknown error")),
              Not(StatusIs(absl::StatusCode::kInvalidArgument, "unknown")));
}

TEST(GutilStatusOrMatcher, StatusIsOkAndHolds) {
  EXPECT_THAT(absl::StatusOr<int>(1320), IsOkAndHolds(1320));
}

TEST(GutilStatusOrMatcher, StatusIsNotOkAndHolds) {
  EXPECT_THAT(absl::StatusOr<int>(1320), Not(IsOkAndHolds(0)));
}

TEST(GutilStatusOrMatcher, StatusIsOkAndHoldsWithExpectation) {
  EXPECT_THAT(absl::StatusOr<std::string>("The quick brown fox"),
              IsOkAndHolds(HasSubstr("fox")));
}

}  // namespace
}  // namespace gutil
