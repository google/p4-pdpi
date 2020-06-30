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

#include "p4_pdpi/utils/ir.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gutil/proto.h"
#include "gutil/testing.h"

using ::google::protobuf::util::MessageDifferencer;

namespace pdpi {

TEST(StringToIrValueTest, Okay) {
  std::vector<std::tuple<std::string, Format, std::string>> testcases = {
      {"abc", Format::STRING, R"pb(str: "abc")pb"},
      {"abc", Format::IPV4, R"pb(ipv4: "abc")pb"},
      {"abc", Format::IPV6, R"pb(ipv6: "abc")pb"},
      {"abc", Format::MAC, R"pb(mac: "abc")pb"},
      {"abc", Format::HEX_STRING, R"pb(hex_str: "abc")pb"},
  };
  for (const auto& [value, format, proto] : testcases) {
    ASSERT_OK_AND_ASSIGN(auto actual, FormattedStringToIrValue(value, format));
    IrValue expected;
    ASSERT_OK(gutil::ReadProtoFromString(proto, &expected));
    EXPECT_TRUE(MessageDifferencer::Equals(actual, expected));
  }
}

TEST(StringToIrValueTest, InvalidFormatFails) {
  ASSERT_FALSE(FormattedStringToIrValue("abc", (Format)-1).ok());
}

TEST(UintToPiByteStringTest, ValidBitwidthValues) {
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(49, 1));
  EXPECT_EQ(value, std::string("1"));
}

TEST(UintToPiByteStringTest, InvalidBitwidth) {
  EXPECT_EQ(pdpi::UintToPiByteString(1, 0).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(pdpi::UintToPiByteString(1, 65).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(UintToPiByteStringTest, Valid8BitwidthValue) {
  const std::string expected = {"\x11"};
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(0x11, 8));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringTest, Valid16BitwidthValue) {
  const std::string expected = "\x11\x22";
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(0x1122, 16));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringTest, Valid32BitwidthValue) {
  const std::string expected = "\x11\x22\x33\x44";
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(0x11223344, 32));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringTest, Valid64BitwidthValue) {
  const std::string expected = "\x11\x22\x33\x44\x55\x66\x77\x88";
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToPiByteString(0x1122334455667788, 64));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid8BitwidthValue) {
  uint64_t expected = 0x11;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 8));
  auto status_or_value = pdpi::PiByteStringToUint(str_value, 8);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid16BitwidthValue) {
  uint64_t expected = 0x1122;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 16));
  auto status_or_value = pdpi::PiByteStringToUint(str_value, 16);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid32BitwidthValue) {
  uint64_t expected = 0x11223344;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 32));
  auto status_or_value = pdpi::PiByteStringToUint(str_value, 32);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid64BitwidthValue) {
  uint64_t expected = 0x1122334455667788;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 64));
  auto status_or_value = pdpi::PiByteStringToUint(str_value, 64);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid8BitwidthValue) {
  const std::string expected = "1";
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(expected, 8));
  auto status_or_value = pdpi::UintToPiByteString(value, 8);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid16BitwidthValue) {
  const std::string expected = "12";
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(expected, 16));
  auto status_or_value = pdpi::UintToPiByteString(value, 16);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid32BitwidthValue) {
  const std::string expected = "1234";
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(expected, 32));
  auto status_or_value = pdpi::UintToPiByteString(value, 32);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid64BitwidthValue) {
  const std::string expected = "12345678";
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(expected, 64));
  auto status_or_value = pdpi::UintToPiByteString(value, 64);
  EXPECT_EQ(status_or_value.status().code(), absl::StatusCode::kOk);
  EXPECT_EQ(status_or_value.value(), expected);
}

}  // namespace pdpi
