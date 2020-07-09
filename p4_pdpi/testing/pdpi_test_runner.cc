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
#include "gutil/proto.h"
#include "gutil/status.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/pd.h"
#include "p4_pdpi/testing/testing.pb.h"
#include "tools/cpp/runfiles/runfiles.h"

ABSL_FLAG(std::string, tests, "", "tests file (required)");

constexpr char kUsage[] = "--tests=<file>";
constexpr char kBanner[] =
    "=========================================================================";
constexpr char kSmallBanner[] =
    "-------------------------------------------------------------------------";

using bazel::tools::cpp::runfiles::Runfiles;
using ::p4::config::v1::P4Info;

gutil::StatusOr<std::string> TestName(const pdpi::Test& test) {
  if (test.has_info_test()) return std::string("InfoTest");
  if (test.has_table_entry_test()) return std::string("TableEntryTest");
  if (test.has_packet_io_test()) return std::string("PacketIoTest");
  return gutil::InvalidArgumentErrorBuilder() << "Invalid test";
}

// Resolves a direct or indirect P4Info.
gutil::StatusOr<P4Info> GetP4Info(const Runfiles* runfiles,
                                  const pdpi::P4Info& p4info) {
  if (p4info.has_direct()) return p4info.direct();

  P4Info info;
  RETURN_IF_ERROR(gutil::ReadProtoFromFile(
      runfiles->Rlocation(absl::StrCat("p4_pdpi/", p4info.indirect())), &info));
  return info;
}

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(
      absl::StrJoin({"usage:", (const char*)argv[0], kUsage}, " "));
  absl::ParseCommandLine(argc, argv);

  // Runfiles init.
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
  if (runfiles == nullptr) {
    std::cerr << "Failed to initialize runfiles: " << error << std::endl;
    return 1;
  }

  // Read tests flag.
  const std::string tests_filename = absl::GetFlag(FLAGS_tests);
  if (tests_filename.empty()) {
    std::cerr << "Missing argument: --tests=<file>" << std::endl;
    return 1;
  }

  // Parse tests file.
  pdpi::Tests tests;
  absl::Status status = gutil::ReadProtoFromFile(tests_filename, &tests);
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
        gutil::StatusOr<P4Info> status_or_p4info =
            GetP4Info(runfiles.get(), test.info_test().p4info());
        if (!status_or_p4info.ok()) {
          std::cerr << status_or_p4info.status() << std::endl;
          return 1;
        }
        P4Info p4info = status_or_p4info.value();
        std::cout << p4info.DebugString() << std::endl;
        gutil::StatusOr<pdpi::IrP4Info> status_or_info =
            pdpi::CreateIrP4Info(p4info);
        if (!status_or_info.ok()) {
          std::cerr << "Test failed with error:" << std::endl;
          std::cerr << status_or_info.status() << std::endl;
        } else {
          std::cout << status_or_info.value().DebugString() << std::endl;
        }
        break;
      }
      case pdpi::Test::KindCase::kTableEntryTest: {
        gutil::StatusOr<P4Info> status_or_p4info =
            GetP4Info(runfiles.get(), test.table_entry_test().p4info());
        if (!status_or_p4info.ok()) {
          std::cerr << status_or_p4info.status() << std::endl;
          return 1;
        }
        P4Info p4info = status_or_p4info.value();

        gutil::StatusOr<pdpi::IrP4Info> status_or_info =
            pdpi::CreateIrP4Info(p4info);
        if (!status_or_info.ok()) {
          std::cerr << status_or_info.status() << std::endl;
          return 1;
        }
        pdpi::IrP4Info info = status_or_info.value();

        for (const pdpi::PiTableEntryCase& pi_case :
             test.table_entry_test().pi_table_entry_cases()) {
          std::cout << kSmallBanner << std::endl;
          std::cout << pi_case.name() << std::endl;
          std::cout << kSmallBanner << std::endl << std::endl;

          std::cout << pi_case.pi().DebugString() << std::endl;
          auto status_or = PiTableEntryToIr(info, pi_case.pi());
          if (status_or.ok()) {
            std::cout << status_or.value().DebugString();
          } else {
            std::cerr << "Subtest failed with error:" << std::endl;
            std::cerr << "  " << status_or.status() << std::endl;
          }
        }
        break;
      }
      case pdpi::Test::KindCase::kPacketIoTest: {
        gutil::StatusOr<P4Info> status_or_p4info =
            GetP4Info(runfiles.get(), test.packet_io_test().p4info());
        if (!status_or_p4info.ok()) {
          std::cerr << status_or_p4info.status() << std::endl;
          return 1;
        }
        P4Info p4info = status_or_p4info.value();

        gutil::StatusOr<pdpi::IrP4Info> status_or_info =
            pdpi::CreateIrP4Info(p4info);
        if (!status_or_info.ok()) {
          std::cerr << status_or_info.status() << std::endl;
          return 1;
        }
        pdpi::IrP4Info info = status_or_info.value();

        for (const pdpi::PiPacketInCase& pi_case :
             test.packet_io_test().pi_packet_in_cases()) {
          std::cout << kSmallBanner << std::endl;
          std::cout << pi_case.name() << std::endl;
          std::cout << kSmallBanner << std::endl << std::endl;

          std::cout << pi_case.pi().DebugString() << std::endl;
          auto status_or = PiPacketInToIr(info, pi_case.pi());
          if (status_or.ok()) {
            std::cout << status_or.value().DebugString();
          } else {
            std::cerr << "Subtest failed with error:" << std::endl;
            std::cerr << "  " << status_or.status() << std::endl;
          }
        }

        for (const pdpi::IrPacketInCase& ir_case :
             test.packet_io_test().ir_packet_in_cases()) {
          std::cout << kSmallBanner << std::endl;
          std::cout << ir_case.name() << std::endl;
          std::cout << kSmallBanner << std::endl << std::endl;

          std::cout << ir_case.ir().DebugString() << std::endl;
          auto status_or = IrPacketInToPi(info, ir_case.ir());
          if (status_or.ok()) {
            std::cout << status_or.value().DebugString();
          } else {
            std::cerr << "Subtest failed with error:" << std::endl;
            std::cerr << "  " << status_or.status() << std::endl;
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
