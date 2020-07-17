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

#include <gtest/gtest.h>

#include "google/protobuf/util/message_differencer.h"
#include "gutil/proto.h"
#include "gutil/testing.h"

namespace pdpi {

using ::google::protobuf::util::MessageDifferencer;

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
  std::string value;
  ASSERT_OK_AND_ASSIGN(value, pdpi::UintToPiByteString(49, 1));
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
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(str_value, 8));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid16BitwidthValue) {
  uint64_t expected = 0x1122;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 16));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(str_value, 16));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid32BitwidthValue) {
  uint64_t expected = 0x11223344;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 32));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(str_value, 32));
  EXPECT_EQ(value, expected);
}

TEST(UintToPiByteStringAndReverseTest, Valid64BitwidthValue) {
  uint64_t expected = 0x1122334455667788;
  ASSERT_OK_AND_ASSIGN(auto str_value, pdpi::UintToPiByteString(expected, 64));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::PiByteStringToUint(str_value, 64));
  EXPECT_EQ(value, expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid8BitwidthValue) {
  const std::string expected = "1";
  ASSERT_OK_AND_ASSIGN(auto uint_value, pdpi::PiByteStringToUint(expected, 8));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(uint_value, 8));
  EXPECT_EQ(value, expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid16BitwidthValue) {
  const std::string expected = "12";
  ASSERT_OK_AND_ASSIGN(auto uint_value, pdpi::PiByteStringToUint(expected, 16));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(uint_value, 16));
  EXPECT_EQ(value, expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid32BitwidthValue) {
  const std::string expected = "1234";
  ASSERT_OK_AND_ASSIGN(auto uint_value, pdpi::PiByteStringToUint(expected, 32));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(uint_value, 32));
  EXPECT_EQ(value, expected);
}

TEST(PiByteStringToUintAndReverseTest, Valid64BitwidthValue) {
  const std::string expected = "12345678";
  ASSERT_OK_AND_ASSIGN(auto uint_value, pdpi::PiByteStringToUint(expected, 64));
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToPiByteString(uint_value, 64));
  EXPECT_EQ(value, expected);
}

TEST(PiByteStringToMacAndReverseTest, ValidMac) {
  const std::string expected1("\x00\x11\x22\x33\x44\x55", 6);
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::PiByteStringToMac(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, MacToPiByteString(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2("\x00\x00\x00\x00\x00\x00", 6);
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::PiByteStringToMac(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, MacToPiByteString(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3("\x0a\x0b\x0c\x0d\x0e\x0f", 6);
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::PiByteStringToMac(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, MacToPiByteString(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(PiByteStringToMacTest, InvalidMac) {
  const std::string expected1 = "\x11\x00\x22\x33\x44\x55";
  EXPECT_EQ(pdpi::PiByteStringToMac(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2("\x11\x22\x33\x44\x55\x66", 5);
  EXPECT_EQ(pdpi::PiByteStringToMac(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(MacToPiByteStringAndReverseTest, ValidMac) {
  const std::string expected1 = "00:11:22:33:44:55";
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::MacToPiByteString(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, pdpi::PiByteStringToMac(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2 = "00:00:00:00:00:00";
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::MacToPiByteString(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, pdpi::PiByteStringToMac(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3 = "0a:0b:0c:0d:0e:0f";
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::MacToPiByteString(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, pdpi::PiByteStringToMac(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(MacToPiByteStringTest, InvalidMac) {
  const std::string expected1 = "abc";
  EXPECT_EQ(pdpi::MacToPiByteString(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2 = "a:b:c:d:e:f";
  EXPECT_EQ(pdpi::MacToPiByteString(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected3 = "b:c:d:e:f";
  EXPECT_EQ(pdpi::MacToPiByteString(expected3).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected4 = "a::b:c:d:e:f";
  EXPECT_EQ(pdpi::MacToPiByteString(expected4).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected5 = "0A:0B:0C:0D:0E:0F";
  EXPECT_EQ(pdpi::MacToPiByteString(expected5).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(PiByteStringToIpv4AndReverseTest, ValidIpv4) {
  const std::string expected1("\x11\x22\x33\x44", 4);
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::PiByteStringToIpv4(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, Ipv4ToPiByteString(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2("\x00\x00\x00\x00", 4);
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::PiByteStringToIpv4(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, Ipv4ToPiByteString(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3("\x0a\x0b\x0c\x0d", 4);
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::PiByteStringToIpv4(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, Ipv4ToPiByteString(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(PiByteStringToIpv4Test, InvalidIpv4) {
  const std::string expected1 = "\x11\x00\x22\x33";
  EXPECT_EQ(pdpi::PiByteStringToIpv4(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2("\x11\x22\x33\x44", 5);
  EXPECT_EQ(pdpi::PiByteStringToIpv4(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(Ipv4ToPiByteStringAndReverseTest, ValidIpv4) {
  const std::string expected1 = "17.34.51.68";
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::Ipv4ToPiByteString(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, pdpi::PiByteStringToIpv4(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2 = "0.0.0.0";
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::Ipv4ToPiByteString(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, pdpi::PiByteStringToIpv4(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3 = "150.53.135.43";
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::Ipv4ToPiByteString(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, pdpi::PiByteStringToIpv4(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(Ipv4ToPiByteStringTest, InvalidIpv4) {
  const std::string expected1 = "abc";
  EXPECT_EQ(pdpi::Ipv4ToPiByteString(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2 = "a:b:c:d:e:f";
  EXPECT_EQ(pdpi::Ipv4ToPiByteString(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected3 = "a.b.c.d";
  EXPECT_EQ(pdpi::Ipv4ToPiByteString(expected3).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected4 = "1..2.3.4";
  EXPECT_EQ(pdpi::Ipv4ToPiByteString(expected4).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(PiByteStringToIpv6AndReverseTest, ValidIpv6) {
  const std::string expected1(
      "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff", 16);
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::PiByteStringToIpv6(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, Ipv6ToPiByteString(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2("\x00\x00\x00\x00", 16);
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::PiByteStringToIpv6(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, Ipv6ToPiByteString(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3("\x0a\x0b\x0c\x0d", 16);
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::PiByteStringToIpv6(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, Ipv6ToPiByteString(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(PiByteStringToIpv6Test, InvalidIpv6) {
  const std::string expected1 = "\x11\x00\x22\x33";
  EXPECT_EQ(pdpi::PiByteStringToIpv6(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2("\x11\x22\x33\x44", 11);
  EXPECT_EQ(pdpi::PiByteStringToIpv6(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(Ipv6ToPiByteStringAndReverseTest, ValidIpv6) {
  const std::string expected1 = "::abcd";
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::Ipv6ToPiByteString(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, pdpi::PiByteStringToIpv6(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2 = "0:abcd::";
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::Ipv6ToPiByteString(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, pdpi::PiByteStringToIpv6(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3 = "ef23:1234:5345::";
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::Ipv6ToPiByteString(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, pdpi::PiByteStringToIpv6(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(Ipv6ToPiByteStringTest, InvalidIpv6) {
  const std::string expected1 = "abc";
  EXPECT_EQ(pdpi::Ipv6ToPiByteString(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2 = "a:b:c:d:e:f";
  EXPECT_EQ(pdpi::Ipv6ToPiByteString(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected3 = "::2342::";
  EXPECT_EQ(pdpi::Ipv6ToPiByteString(expected3).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected4 = "gr:we:hgnf:kjo";
  EXPECT_EQ(pdpi::Ipv6ToPiByteString(expected4).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected5 = "::ABcd";
  EXPECT_EQ(pdpi::Ipv6ToPiByteString(expected5).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(GetFormatTest, MacAnnotationPass) {
  std::vector<std::string> annotations = {"@format(MAC_ADDRESS)"};
  ASSERT_OK_AND_ASSIGN(auto format, GetFormat(annotations, kNumBitsInMac,
                                              /*is_sdn_string=*/false))
  EXPECT_EQ(format, Format::MAC);
}

TEST(GetFormatTest, MacAnnotationInvalidBitwidth) {
  std::vector<std::string> annotations = {"@format(MAC_ADDRESS)"};
  auto status_or_format =
      GetFormat(annotations, /*bitwidth=*/65, /*is_sdn_string=*/false);
  EXPECT_EQ(status_or_format.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(GetFormatTest, Ipv4AnnotationPass) {
  std::vector<std::string> annotations = {"@format(IPV4_ADDRESS)"};
  ASSERT_OK_AND_ASSIGN(auto format, GetFormat(annotations, kNumBitsInIpv4,
                                              /*is_sdn_string=*/false))
  EXPECT_EQ(format, Format::IPV4);
}

TEST(GetFormatTest, Ipv4AnnotationInvalidBitwidth) {
  std::vector<std::string> annotations = {"@format(IPV4_ADDRESS)"};
  auto status_or_format =
      GetFormat(annotations, /*bitwidth=*/65, /*is_sdn_string=*/false);
  EXPECT_EQ(status_or_format.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(GetFormatTest, Ipv6AnnotationPass) {
  std::vector<std::string> annotations = {"@format(IPV6_ADDRESS)"};
  ASSERT_OK_AND_ASSIGN(auto format, GetFormat(annotations, kNumBitsInIpv6,
                                              /*is_sdn_string=*/false))
  EXPECT_EQ(format, Format::IPV6);
}

TEST(GetFormatTest, Ipv6AnnotationInvalidBitwidth) {
  std::vector<std::string> annotations = {"@format(IPV6_ADDRESS)"};
  auto status_or_format =
      GetFormat(annotations, /*bitwidth=*/65, /*is_sdn_string=*/false);
  EXPECT_EQ(status_or_format.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(GetFormatTest, ConflictingAnnotations) {
  std::vector<std::string> annotations = {"@format(IPV6_ADDRESS)",
                                          "@format(IPV4_ADDRESS)"};
  auto status_or_format =
      GetFormat(annotations, /*bitwidth=*/65, /*is_sdn_string=*/false);
  EXPECT_EQ(status_or_format.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(GetFormatTest, SdnStringFormat) {
  std::vector<std::string> annotations = {};
  ASSERT_OK_AND_ASSIGN(auto format, GetFormat(annotations, /*bitwidth=*/65,
                                              /*is_sdn_string=*/true));
  EXPECT_EQ(format, Format::STRING);
}

TEST(GetFormatTest, SdnStringFormatConflictingAnnotations) {
  std::vector<std::string> annotations = {"@format(IPV4_ADDRESS)"};
  auto status_or_format =
      GetFormat(annotations, /*bitwidth=*/65, /*is_sdn_string=*/true);
  EXPECT_EQ(status_or_format.status().code(),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace pdpi
