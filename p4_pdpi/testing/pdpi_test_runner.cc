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
#include "p4_pdpi/ir.h"
#include "p4_pdpi/pdpi.h"
#include "p4_pdpi/testing/testing.pb.h"
#include "p4_pdpi/util.h"
#include "p4_pdpi/utils/status_utils.h"

ABSL_FLAG(std::string, tests, "", "tests file (required)");

constexpr char kUsage[] = "--tests=<file>";
constexpr char kBanner[] =
    "=========================================================================";
constexpr char kSmallBanner[] =
    "-------------------------------------------------------------------------";

using ::p4::config::v1::P4Info;

pdpi::StatusOr<std::string> TestName(const pdpi::Test& test) {
  if (test.has_info_test()) return std::string("InfoTest");
  if (test.has_table_entry_test()) return std::string("TableEntryTest");
  return pdpi::InvalidArgumentErrorBuilder() << "Invalid test";
}

// Resolves a direct or indirect P4Info.
pdpi::StatusOr<P4Info> GetP4Info(const pdpi::P4Info& p4info) {
  if (p4info.has_direct()) return p4info.direct();

  P4Info info;
  RETURN_IF_ERROR(pdpi::ReadProtoFromFile(p4info.indirect(), &info));
  return info;
}

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(
      absl::StrJoin({"usage:", (const char*)argv[0], kUsage}, " "));
  absl::ParseCommandLine(argc, argv);

  // Read tests flag.
  const std::string tests_filename = absl::GetFlag(FLAGS_tests);
  if (tests_filename.empty()) {
    std::cerr << "Missing argument: --tests=<file>" << std::endl;
    return 1;
  }

  // Parse tests file.
  pdpi::Tests tests;
  absl::Status status = pdpi::ReadProtoFromFile(tests_filename, &tests);
  if (!status.ok()) {
    std::cerr << status << std::endl;
    return 1;
  }

  // Iterate over all tests.
  for (const pdpi::Test& test : tests.tests()) {
    std::cout << kBanner << std::endl;
    auto status_or_test_name = TestName(test);
    if (!status_or_test_name.ok()) {
      std::cerr << status_or_test_name.status() << std::endl;
      return 1;
    }
    std::cout << status_or_test_name.value() << ": " << test.name()
              << std::endl;
    std::cout << kBanner << std::endl << std::endl;
    switch (test.kind_case()) {
      case pdpi::Test::KindCase::kInfoTest: {
        pdpi::StatusOr<P4Info> status_or_p4info =
            GetP4Info(test.info_test().p4info());
        if (!status_or_p4info.ok()) {
          std::cerr << status_or_p4info.status() << std::endl;
          return 1;
        }
        P4Info p4info = status_or_p4info.value();
        std::cout << p4info.DebugString() << std::endl;
        pdpi::StatusOr<std::unique_ptr<pdpi::P4InfoManager>> status_or_info =
            pdpi::P4InfoManager::Create(p4info);
        if (!status_or_info.ok()) {
          std::cerr << "Test failed with error:" << std::endl;
          std::cerr << status_or_info.status() << std::endl;
        } else {
          std::cout << status_or_info.value()->GetIrP4Info().DebugString()
                    << std::endl;
        }
        break;
      }
      case pdpi::Test::KindCase::kTableEntryTest: {
        pdpi::StatusOr<P4Info> status_or_p4info =
            GetP4Info(test.table_entry_test().p4info());
        if (!status_or_p4info.ok()) {
          std::cerr << status_or_p4info.status() << std::endl;
          return 1;
        }
        P4Info p4info = status_or_p4info.value();

        pdpi::StatusOr<std::unique_ptr<pdpi::P4InfoManager>> status_or_info =
            pdpi::P4InfoManager::Create(p4info);
        if (!status_or_info.ok()) {
          std::cerr << status_or_p4info.status() << std::endl;
          return 1;
        }

        for (const pdpi::PiTableEntryCase& pi_case :
             test.table_entry_test().pi_table_entry_cases()) {
          std::cout << kSmallBanner << std::endl;
          std::cout << pi_case.name() << std::endl;
          std::cout << kSmallBanner << std::endl << std::endl;

          std::cout << pi_case.pi().DebugString() << std::endl;
          auto status_or_table_entry =
              status_or_info.value()->PiTableEntryToIr(pi_case.pi());
          if (status_or_table_entry.ok()) {
            std::cout << status_or_table_entry.value().DebugString();
          } else {
            std::cerr << "Subtest failed with error:" << std::endl;
            std::cerr << "  " << status_or_table_entry.status() << std::endl;
          }
        }
        break;
      }
      default:
        std::cout << "Empty test, nothing to do." << std::endl;
        break;
    }

    std::cout << std::endl << std::endl;
  }

  return 0;
}
