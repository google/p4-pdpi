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

#include <stdint.h>

#include <string>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "gutil/proto.h"
#include "gutil/status.h"
#include "gutil/status_matchers.h"
#include "p4_pdpi/ir.pb.h"

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

TEST(UintToNormalizedByteStringTest, ValidBitwidthValues) {
  std::string value;
  ASSERT_OK_AND_ASSIGN(value, pdpi::UintToNormalizedByteString(1, 1));
  EXPECT_EQ(value, std::string("\x1"));
}

TEST(UintToNormalizedByteStringTest, InvalidBitwidth) {
  EXPECT_EQ(pdpi::UintToNormalizedByteString(1, 0).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(pdpi::UintToNormalizedByteString(1, 65).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(UintToNormalizedByteStringTest, Valid8BitwidthValue) {
  const std::string expected = {"\x11"};
  ASSERT_OK_AND_ASSIGN(auto value, pdpi::UintToNormalizedByteString(0x11, 8));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringTest, Valid16BitwidthValue) {
  const std::string expected = "\x11\x22";
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToNormalizedByteString(0x1122, 16));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringTest, Valid32BitwidthValue) {
  const std::string expected = "\x11\x22\x33\x44";
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToNormalizedByteString(0x11223344, 32));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringTest, Valid64BitwidthValue) {
  const std::string expected = "\x11\x22\x33\x44\x55\x66\x77\x88";
  ASSERT_OK_AND_ASSIGN(
      auto value, pdpi::UintToNormalizedByteString(0x1122334455667788, 64));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringAndReverseTest, Valid8BitwidthValue) {
  uint64_t expected = 0x11;
  ASSERT_OK_AND_ASSIGN(auto str_value,
                       pdpi::UintToNormalizedByteString(expected, 8));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::ArbitraryByteStringToUint(str_value, 8));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringAndReverseTest, Valid16BitwidthValue) {
  uint64_t expected = 0x1122;
  ASSERT_OK_AND_ASSIGN(auto str_value,
                       pdpi::UintToNormalizedByteString(expected, 16));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::ArbitraryByteStringToUint(str_value, 16));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringAndReverseTest, Valid32BitwidthValue) {
  uint64_t expected = 0x11223344;
  ASSERT_OK_AND_ASSIGN(auto str_value,
                       pdpi::UintToNormalizedByteString(expected, 32));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::ArbitraryByteStringToUint(str_value, 32));
  EXPECT_EQ(value, expected);
}

TEST(UintToNormalizedByteStringAndReverseTest, Valid64BitwidthValue) {
  uint64_t expected = 0x1122334455667788;
  ASSERT_OK_AND_ASSIGN(auto str_value,
                       pdpi::UintToNormalizedByteString(expected, 64));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::ArbitraryByteStringToUint(str_value, 64));
  EXPECT_EQ(value, expected);
}

TEST(ArbitraryByteStringToUintAndReverseTest, Valid8BitwidthValue) {
  const std::string expected = "1";
  ASSERT_OK_AND_ASSIGN(auto uint_value,
                       pdpi::ArbitraryByteStringToUint(expected, 8));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToNormalizedByteString(uint_value, 8));
  EXPECT_EQ(value, expected);
}

TEST(ArbitraryByteStringToUintAndReverseTest, Valid16BitwidthValue) {
  const std::string expected = "12";
  ASSERT_OK_AND_ASSIGN(auto uint_value,
                       pdpi::ArbitraryByteStringToUint(expected, 16));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToNormalizedByteString(uint_value, 16));
  EXPECT_EQ(value, expected);
}

TEST(ArbitraryByteStringToUintAndReverseTest, Valid32BitwidthValue) {
  const std::string expected = "1234";
  ASSERT_OK_AND_ASSIGN(auto uint_value,
                       pdpi::ArbitraryByteStringToUint(expected, 32));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToNormalizedByteString(uint_value, 32));
  EXPECT_EQ(value, expected);
}

TEST(ArbitraryByteStringToUintAndReverseTest, Valid64BitwidthValue) {
  const std::string expected = "12345678";
  ASSERT_OK_AND_ASSIGN(auto uint_value,
                       pdpi::ArbitraryByteStringToUint(expected, 64));
  ASSERT_OK_AND_ASSIGN(auto value,
                       pdpi::UintToNormalizedByteString(uint_value, 64));
  EXPECT_EQ(value, expected);
}

TEST(NormalizedByteStringToMacAndReverseTest, ValidMac) {
  const std::string expected1("\x00\x11\x22\x33\x44\x55", 6);
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::NormalizedByteStringToMac(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, MacToNormalizedByteString(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2("\x00\x00\x00\x00\x00\x00", 6);
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::NormalizedByteStringToMac(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, MacToNormalizedByteString(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3("\x0a\x0b\x0c\x0d\x0e\x0f", 6);
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::NormalizedByteStringToMac(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, MacToNormalizedByteString(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(NormalizedByteStringToMacTest, InvalidMac) {
  const std::string expected1 = "\x11\x00\x22\x33\x44\x55";
  EXPECT_EQ(pdpi::NormalizedByteStringToMac(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2("\x11\x22\x33\x44\x55\x66", 5);
  EXPECT_EQ(pdpi::NormalizedByteStringToMac(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(MacToNormalizedByteStringAndReverseTest, ValidMac) {
  const std::string expected1 = "00:11:22:33:44:55";
  ASSERT_OK_AND_ASSIGN(auto value1, pdpi::MacToNormalizedByteString(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1,
                       pdpi::NormalizedByteStringToMac(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2 = "00:00:00:00:00:00";
  ASSERT_OK_AND_ASSIGN(auto value2, pdpi::MacToNormalizedByteString(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2,
                       pdpi::NormalizedByteStringToMac(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3 = "0a:0b:0c:0d:0e:0f";
  ASSERT_OK_AND_ASSIGN(auto value3, pdpi::MacToNormalizedByteString(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3,
                       pdpi::NormalizedByteStringToMac(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(MacToNormalizedByteStringTest, InvalidMac) {
  const std::string expected1 = "abc";
  EXPECT_EQ(pdpi::MacToNormalizedByteString(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2 = "a:b:c:d:e:f";
  EXPECT_EQ(pdpi::MacToNormalizedByteString(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected3 = "b:c:d:e:f";
  EXPECT_EQ(pdpi::MacToNormalizedByteString(expected3).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected4 = "a::b:c:d:e:f";
  EXPECT_EQ(pdpi::MacToNormalizedByteString(expected4).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected5 = "0A:0B:0C:0D:0E:0F";
  EXPECT_EQ(pdpi::MacToNormalizedByteString(expected5).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(NormalizedByteStringToIpv4AndReverseTest, ValidIpv4) {
  const std::string expected1("\x11\x22\x33\x44", 4);
  ASSERT_OK_AND_ASSIGN(auto value1,
                       pdpi::NormalizedByteStringToIpv4(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, Ipv4ToNormalizedByteString(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2("\x00\x00\x00\x00", 4);
  ASSERT_OK_AND_ASSIGN(auto value2,
                       pdpi::NormalizedByteStringToIpv4(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, Ipv4ToNormalizedByteString(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3("\x0a\x0b\x0c\x0d", 4);
  ASSERT_OK_AND_ASSIGN(auto value3,
                       pdpi::NormalizedByteStringToIpv4(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, Ipv4ToNormalizedByteString(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(NormalizedByteStringToIpv4Test, InvalidIpv4) {
  const std::string expected1 = "\x11\x00\x22\x33";
  EXPECT_EQ(pdpi::NormalizedByteStringToIpv4(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2("\x00\x11\x22\x33\x44", 5);
  EXPECT_EQ(pdpi::NormalizedByteStringToIpv4(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(Ipv4ToNormalizedByteStringAndReverseTest, ValidIpv4) {
  const std::string expected1 = "17.34.51.68";
  ASSERT_OK_AND_ASSIGN(auto value1,
                       pdpi::Ipv4ToNormalizedByteString(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1,
                       pdpi::NormalizedByteStringToIpv4(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2 = "0.0.0.0";
  ASSERT_OK_AND_ASSIGN(auto value2,
                       pdpi::Ipv4ToNormalizedByteString(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2,
                       pdpi::NormalizedByteStringToIpv4(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3 = "150.53.135.43";
  ASSERT_OK_AND_ASSIGN(auto value3,
                       pdpi::Ipv4ToNormalizedByteString(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3,
                       pdpi::NormalizedByteStringToIpv4(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(Ipv4ToNormalizedByteStringTest, InvalidIpv4) {
  const std::string expected1 = "abc";
  EXPECT_EQ(pdpi::Ipv4ToNormalizedByteString(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2 = "a:b:c:d:e:f";
  EXPECT_EQ(pdpi::Ipv4ToNormalizedByteString(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected3 = "a.b.c.d";
  EXPECT_EQ(pdpi::Ipv4ToNormalizedByteString(expected3).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected4 = "1..2.3.4";
  EXPECT_EQ(pdpi::Ipv4ToNormalizedByteString(expected4).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(NormalizedByteStringToIpv6AndReverseTest, ValidIpv6) {
  const std::string expected1(
      "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff", 16);
  ASSERT_OK_AND_ASSIGN(auto value1,
                       pdpi::NormalizedByteStringToIpv6(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1, Ipv6ToNormalizedByteString(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
  ASSERT_OK_AND_ASSIGN(auto value2,
                       pdpi::NormalizedByteStringToIpv6(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2, Ipv6ToNormalizedByteString(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0a\x0b\x0c\x0d", 16);
  ASSERT_OK_AND_ASSIGN(auto value3,
                       pdpi::NormalizedByteStringToIpv6(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3, Ipv6ToNormalizedByteString(value3));
  EXPECT_EQ(str_value3, expected3);
}

TEST(NormalizedByteStringToIpv6Test, InvalidIpv6) {
  const std::string expected1 = "\x11\x00\x22\x33";
  EXPECT_EQ(pdpi::NormalizedByteStringToIpv6(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2("\x00\x00\x00\x00\x00\x00\x00\x11\x22\x33\x44",
                              11);
  EXPECT_EQ(pdpi::NormalizedByteStringToIpv6(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(Ipv6ToNormalizedByteStringAndReverseTest, ValidIpv6) {
  const std::string expected1 = "::abcd";
  ASSERT_OK_AND_ASSIGN(auto value1,
                       pdpi::Ipv6ToNormalizedByteString(expected1));
  ASSERT_OK_AND_ASSIGN(auto str_value1,
                       pdpi::NormalizedByteStringToIpv6(value1));
  EXPECT_EQ(str_value1, expected1);

  const std::string expected2 = "0:abcd::";
  ASSERT_OK_AND_ASSIGN(auto value2,
                       pdpi::Ipv6ToNormalizedByteString(expected2));
  ASSERT_OK_AND_ASSIGN(auto str_value2,
                       pdpi::NormalizedByteStringToIpv6(value2));
  EXPECT_EQ(str_value2, expected2);

  const std::string expected3 = "ef23:1234:5345::";
  ASSERT_OK_AND_ASSIGN(auto value3,
                       pdpi::Ipv6ToNormalizedByteString(expected3));
  ASSERT_OK_AND_ASSIGN(auto str_value3,
                       pdpi::NormalizedByteStringToIpv6(value3));
  EXPECT_EQ(str_value3, expected3);

  // TODO: Ideally we would like this to print as "::ffff:2222 and not
  // ::255.255.34.34
  const std::string expected4 = "::255.255.34.34";
  ASSERT_OK_AND_ASSIGN(auto value4,
                       pdpi::Ipv6ToNormalizedByteString(expected4));
  ASSERT_OK_AND_ASSIGN(auto str_value4,
                       pdpi::NormalizedByteStringToIpv6(value4));
  EXPECT_EQ(str_value4, expected4);
}

TEST(Ipv6ToNormalizedByteStringTest, InvalidIpv6) {
  const std::string expected1 = "abc";
  EXPECT_EQ(pdpi::Ipv6ToNormalizedByteString(expected1).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected2 = "a:b:c:d:e:f";
  EXPECT_EQ(pdpi::Ipv6ToNormalizedByteString(expected2).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected3 = "::2342::";
  EXPECT_EQ(pdpi::Ipv6ToNormalizedByteString(expected3).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected4 = "gr:we:hgnf:kjo";
  EXPECT_EQ(pdpi::Ipv6ToNormalizedByteString(expected4).status().code(),
            absl::StatusCode::kInvalidArgument);

  const std::string expected5 = "::ABcd";
  EXPECT_EQ(pdpi::Ipv6ToNormalizedByteString(expected5).status().code(),
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

TEST(GetFormatTest, InvalidAnnotations) {
  std::vector<std::string> annotations = {"@format(IPVx_ADDRESS)"};
  auto status_or_format =
      GetFormat(annotations, /*bitwidth=*/65, /*is_sdn_string=*/false);
  EXPECT_EQ(status_or_format.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(IsAllZerosTest, TestZeros) {
  EXPECT_TRUE(IsAllZeros("\x00\x00\x00\x00"));
  EXPECT_FALSE(IsAllZeros("\x01\x00\x00\x00"));
}

TEST(IntersectionTest, UnequalLengths) {
  const auto status_or_result =
      Intersection("\x41\x42\x43", "\x41\x42\x43\x44");
  EXPECT_EQ(status_or_result.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(IntersectionTest, NoChange) {
  std::string expected = "\x41\x42\x43";
  ASSERT_OK_AND_ASSIGN(const auto& result,
                       Intersection(expected, "\xff\xff\xff"));
  EXPECT_EQ(result, expected);
}

TEST(IntersectionTest, AllZeros) {
  std::string input = "\x41\x42\x43";
  ASSERT_OK_AND_ASSIGN(const auto& result,
                       Intersection(input, std::string("\x00\x00\x00", 3)));
  EXPECT_TRUE(IsAllZeros(result));
}

TEST(PrefixLenToMaskTest, PrefixLenTooLong) {
  const auto status_or_result = PrefixLenToMask(33, 32);
  EXPECT_EQ(status_or_result.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(PrefixLenToMaskTest, Ipv4Test) {
  ASSERT_OK_AND_ASSIGN(const auto result, PrefixLenToMask(23, 32));
  std::string expected("\xff\xff\xfe\x00", 4);
  EXPECT_EQ(result, expected);
}

TEST(PrefixLenToMaskTest, Ipv6Test) {
  ASSERT_OK_AND_ASSIGN(const auto result, PrefixLenToMask(96, 128));
  std::string expected(
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00", 16);
  EXPECT_EQ(result, expected);
}

TEST(PrefixLenToMaskTest, GenericValueTest) {
  ASSERT_OK_AND_ASSIGN(const auto result, PrefixLenToMask(23, 33));
  std::string expected("\x01\xff\xff\xfc\x00", 5);
  EXPECT_EQ(result, expected);
}

}  // namespace pdpi
