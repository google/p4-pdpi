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

#include "p4_pdpi/utils/hex_string.h"

#include <bitset>
#include <cstdint>
#include <cstring>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "glog/logging.h"
#include "gutil/status.h"

namespace pdpi {

// -- Conversions from Hex Strings ---------------------------------------------

template <class T>
absl::StatusOr<T> HexStringTo(absl::string_view hex_string) {
  T result;
  ASSIGN_OR_RETURN(
      const std::bitset<8 * sizeof(result)> bits,
      HexStringToAnyLargeEnoughBitset<8 * sizeof(result)>(hex_string));
  const unsigned long long bits_as_ullong = bits.to_ullong();  // NOLINT
  static_assert(sizeof(bits_as_ullong) >= sizeof(result));
  std::memcpy(&result, &bits_as_ullong, sizeof(result));
  return result;
}

absl::StatusOr<int> HexStringToInt(absl::string_view hex_string) {
  return HexStringTo<int>(hex_string);
}

absl::StatusOr<int32_t> HexStringToInt32(absl::string_view hex_string) {
  return HexStringTo<int32_t>(hex_string);
}

absl::StatusOr<int64_t> HexStringToInt64(absl::string_view hex_string) {
  return HexStringTo<int64_t>(hex_string);
}

absl::StatusOr<uint32_t> HexStringToUint32(absl::string_view hex_string) {
  return HexStringTo<uint32_t>(hex_string);
}

absl::StatusOr<uint64_t> HexStringToUint64(absl::string_view hex_string) {
  return HexStringTo<uint64_t>(hex_string);
}

// -- Conversions between Hex Characters and Digits ----------------------------

char HexDigitToChar(int digit) {
  switch (digit) {
    case 0:
      return '0';
    case 1:
      return '1';
    case 2:
      return '2';
    case 3:
      return '3';
    case 4:
      return '4';
    case 5:
      return '5';
    case 6:
      return '6';
    case 7:
      return '7';
    case 8:
      return '8';
    case 9:
      return '9';
    case 10:
      return 'a';
    case 11:
      return 'b';
    case 12:
      return 'c';
    case 13:
      return 'd';
    case 14:
      return 'e';
    case 15:
      return 'f';
  }
  LOG(DFATAL) << "illegal hexadecimal digit: " << digit << "; returning '?'";
  return '?';
}

absl::StatusOr<int> HexCharToDigit(char hex_char) {
  switch (hex_char) {
    case '0':
      return 0;
    case '1':
      return 1;
    case '2':
      return 2;
    case '3':
      return 3;
    case '4':
      return 4;
    case '5':
      return 5;
    case '6':
      return 6;
    case '7':
      return 7;
    case '8':
      return 8;
    case '9':
      return 9;
    case 'a':
    case 'A':
      return 10;
    case 'b':
    case 'B':
      return 11;
    case 'c':
    case 'C':
      return 12;
    case 'd':
    case 'D':
      return 13;
    case 'e':
    case 'E':
      return 14;
    case 'f':
    case 'F':
      return 15;
  }
  return gutil::InvalidArgumentErrorBuilder()
         << "invalid hexadecimal character: " << hex_char;
}

}  // namespace pdpi
