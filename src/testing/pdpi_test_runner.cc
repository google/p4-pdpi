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

// Test runner for pdpi

#include <google/protobuf/text_format.h>

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_join.h"
#include "src/ir.h"
#include "src/pdpi.h"
#include "src/testing/testing.pb.h"
#include "src/util.h"

ABSL_FLAG(std::string, tests, "", "tests file (required)");

constexpr char kUsage[] = "--tests=<file>";
constexpr char kBanner[] =
    "=========================================================================";
constexpr char kSmallBanner[] =
    "-------------------------------------------------------------------------";

using ::p4::config::v1::P4Info;

std::string TestName(const pdpi::Test& test) {
  if (test.has_info_test()) return "InfoTest";
  if (test.has_table_entry_test()) return "TableEntryTest";
  throw std::invalid_argument("Invalid test");
}

// Resolves a direct or indirect P4Info.
P4Info GetP4Info(const pdpi::P4Info& p4info) {
  if (p4info.has_direct()) return p4info.direct();

  P4Info info;
  pdpi::ReadProtoFromFile(p4info.indirect(), &info);
  return info;
}

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(
      absl::StrJoin({"usage:", (const char*)argv[0], kUsage}, " "));
  absl::ParseCommandLine(argc, argv);

  // Read tests flag.
  const std::string tests_filename = absl::GetFlag(FLAGS_tests);
  if (tests_filename.empty()) {
    std::cerr << "Missing argument: --tests=<file>\n";
    return 1;
  }

  // Parse tests file.
  pdpi::Tests tests;
  try {
    pdpi::ReadProtoFromFile(tests_filename, &tests);
  } catch (const std::invalid_argument& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }

  // Iterate over all tests.
  for (const pdpi::Test& test : tests.tests()) {
    std::cout << kBanner << std::endl;
    std::cout << TestName(test) << ": " << test.name() << std::endl;
    std::cout << kBanner << std::endl << std::endl;
    try {
      switch (test.kind_case()) {
        case pdpi::Test::KindCase::kInfoTest: {
          P4Info p4info = GetP4Info(test.info_test().p4info());
          std::cout << p4info.DebugString() << std::endl;
          pdpi::P4InfoManager info(p4info);
          std::cout << info.GetIrP4Info().DebugString() << std::endl;
          break;
        }
        case pdpi::Test::KindCase::kTableEntryTest: {
          P4Info p4info = GetP4Info(test.table_entry_test().p4info());
          pdpi::P4InfoManager info(p4info);

          for (const pdpi::PiTableEntryCase& pi_case :
               test.table_entry_test().pi_table_entry_cases()) {
            std::cout << kSmallBanner << std::endl;
            std::cout << pi_case.name() << std::endl;
            std::cout << kSmallBanner << std::endl << std::endl;

            std::cout << pi_case.pi().DebugString() << std::endl;
            try {
              std::cout << info.PiTableEntryToIr(pi_case.pi()).DebugString()
                        << std::endl;
            } catch (const std::invalid_argument& exception) {
              std::cout << "Subtest failed with error:" << std::endl;
              std::cout << "  " << exception.what() << std::endl;
            }
          }
          break;
        }
        default:
          std::cout << "Empty test, nothing to do." << std::endl;
          break;
      }
    } catch (const std::invalid_argument& exception) {
      std::cout << "Test failed with error:" << std::endl;
      std::cout << "  " << exception.what() << std::endl;
    }

    std::cout << std::endl << std::endl;
  }

  return 0;
}
