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
  if (test.has_metadata_creation_test()) return "MetadataCreateTest";
  if (test.has_ir_test()) return "IrTest";
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
        case pdpi::Test::KindCase::kMetadataCreationTest: {
          P4Info p4info = GetP4Info(test.metadata_creation_test().p4info());
          std::cout << p4info.DebugString() << std::endl;
          std::cout << pdpi::MetadataToString(pdpi::CreateMetadata(p4info))
                    << std::endl;
          break;
        }
        case pdpi::Test::KindCase::kIrTest: {
          P4Info p4info = GetP4Info(test.ir_test().p4info());
          const auto& metadata = pdpi::CreateMetadata(p4info);
          for (const pdpi::PiTableEntryCase& pi_case :
               test.ir_test().pi_table_entry_cases()) {
            std::cout << kSmallBanner << std::endl;
            std::cout << pi_case.name() << std::endl;
            std::cout << kSmallBanner << std::endl << std::endl;

            std::cout << pi_case.pi().DebugString() << std::endl;
            try {
              std::cout << pdpi::IrToString(
                               pdpi::PiToIr(metadata, pi_case.pi()))
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