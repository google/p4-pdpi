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
#include "src/pdpi.h"
#include "src/testing/testing.pb.h"

ABSL_FLAG(std::string, tests, "", "tests file (required)");

constexpr char kUsage[] = "--tests=<file>";
constexpr char kBanner[] =
    "=========================================================================";

using ::p4::config::v1::P4Info;

std::string TestName(const pdpi::Test& test) {
  if (test.has_metadata_creation_test()) return "MetadataCreateTest";
  return "NO_TEST";
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

  // Read tests file.
  std::ifstream tests_file(tests_filename);
  if (!tests_file.is_open()) {
    std::cerr << "Unable to open tests file: " << tests_filename << "\n";
    return 1;
  }

  // Parse tests file.
  pdpi::Tests tests;
  {
    std::string tests_str((std::istreambuf_iterator<char>(tests_file)),
                          std::istreambuf_iterator<char>());
    if (!google::protobuf::TextFormat::ParseFromString(tests_str, &tests)) {
      std::cerr << "Unable to parse tests file: " << tests_filename << "\n";
      return 1;
    }
  }

  // Iterate over all tests.
  for (const pdpi::Test& test : tests.tests()) {
    std::cout << kBanner << std::endl;
    std::cout << TestName(test) << ": " << test.name() << std::endl;
    std::cout << kBanner << std::endl << std::endl;
    try {
      if (test.has_metadata_creation_test()) {
        P4Info p4info = test.metadata_creation_test().p4info();
        std::cout << p4info.DebugString() << std::endl;
        std::cout << pdpi::MetadataToString(pdpi::CreateMetadata(p4info))
                  << std::endl;
      } else {
        std::cout << "Empty test, nothing to do." << std::endl;
      }
    } catch (const std::invalid_argument& exception) {
      std::cout << "Failed with error:" << std::endl;
      std::cout << "  " << exception.what() << std::endl;
    }

    std::cout << std::endl << std::endl;
  }

  return 0;
}