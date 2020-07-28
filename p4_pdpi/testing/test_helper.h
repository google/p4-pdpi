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

#ifndef P4_PDPI_TESTING_TEST_HELPER_H_
#define P4_PDPI_TESTING_TEST_HELPER_H_

#include "google/protobuf/util/message_differencer.h"
#include "gutil/testing.h"
#include "p4_pdpi/ir.h"
#include "tools/cpp/runfiles/runfiles.h"

constexpr char kBanner[] =
    "=========================================================================";

std::string TestHeader(const std::string& test_name) {
  return absl::StrCat(kBanner, "\n", test_name, "\n", kBanner);
}

void Fail(const std::string& message) {
  std::cerr << "TEST FAILED (DO NOT SUBMIT)" << std::endl;
  std::cerr << "FAILURE REASON: " << message << std::endl;
}

p4::config::v1::P4Info GetP4Info(
    const bazel::tools::cpp::runfiles::Runfiles* runfiles,
    const std::string& name) {
  p4::config::v1::P4Info info;
  CHECK_OK(gutil::ReadProtoFromFile(
      runfiles->Rlocation(absl::StrCat("p4_pdpi/", name)), &info));
  return info;
}

// Runs a generic test starting from an invalid PI and checks that it cannot be
// translated to IR. If you want to test valid PI, instead write a generic PD
// test.
template <typename IR, typename PI>
void RunGenericPiTest(
    const pdpi::IrP4Info& info, const std::string& test_name, const PI& pi,
    const std::function<gutil::StatusOr<IR>(const pdpi::IrP4Info&, const PI&)>&
        pi_to_ir) {
  // Input and header.
  std::cout << TestHeader(test_name) << std::endl << std::endl;
  std::cout << "--- PI (Input):" << std::endl;
  std::cout << pi.DebugString() << std::endl;

  // Convert PI to IR.
  const auto& status_or_ir = pi_to_ir(info, pi);
  if (!status_or_ir.ok()) {
    std::cout << "--- PI is invalid/unsupported:" << std::endl;
    std::cout << status_or_ir.status() << std::endl;
  } else {
    Fail(
        "Expected PI to be invalid (valid PI should instead be tested using "
        "RunGenericPdTest.");
  }
  std::cout << std::endl;
}

// Runs a generic test starting from a valid PD entity. If pd is valid, then it
// is translated: PD -> IR -> PI -> IR2 -> PD2 and IR == IR2 and PD == PD2 are
// checked.
template <typename PD, typename IR, typename PI>
void RunGenericPdTest(
    const pdpi::IrP4Info& info, const std::string& test_name, const PD& pd,
    const std::function<gutil::StatusOr<IR>(const pdpi::IrP4Info&, const PD&)>&
        pd_to_ir,
    const std::function<absl::Status(const pdpi::IrP4Info&, const IR&,
                                     google::protobuf::Message*)>& ir_to_pd,
    const std::function<gutil::StatusOr<PI>(const pdpi::IrP4Info&, const IR&)>&
        ir_to_pi,
    const std::function<gutil::StatusOr<IR>(const pdpi::IrP4Info&, const PI&)>&
        pi_to_ir) {
  // Input and header.
  std::cout << TestHeader(test_name) << std::endl << std::endl;
  std::cout << "--- PD (Input):" << std::endl;
  std::cout << pd.DebugString() << std::endl;

  // Set-up message differencer.
  google::protobuf::util::MessageDifferencer diff;
  diff.set_report_moves(false);
  diff.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::RepeatedFieldComparison::
          AS_SET);
  std::string explanation;
  diff.ReportDifferencesToString(&explanation);

  // Convert PD to IR.
  const auto& status_or_ir = pd_to_ir(info, pd);
  if (!status_or_ir.ok()) {
    std::cout << "--- PD is invalid/unsupported:" << std::endl;
    std::cout << status_or_ir.status() << std::endl;
  } else {
    const auto& ir = status_or_ir.value();
    std::cout << "--- IR:" << std::endl;
    std::cout << ir.DebugString() << std::endl;

    // Convert IR to PI.
    const auto& status_or_pi = ir_to_pi(info, ir);
    if (!status_or_pi.status().ok()) {
      Fail("Translation from IR to PI failed, even though PD to IR succeeded.");
      std::cout << status_or_pi.status().message() << std::endl;
      return;
    }
    const auto& pi = status_or_pi.value();
    std::cout << "--- PI:" << std::endl;
    std::cout << pi.DebugString() << std::endl;

    // Convert PI back to IR.
    const auto& status_or_ir2 = pi_to_ir(info, pi);
    if (!status_or_ir2.status().ok()) {
      Fail("Reverse translation from PI to IR failed.");
      std::cout << status_or_ir2.status().message() << std::endl;
      return;
    }
    if (!diff.Compare(ir, status_or_ir2.value())) {
      Fail("Reverse translation from PI to IR resulted in a different IR.");
      std::cout << "Differences: " << explanation << std::endl;
      std::cout << "IR (after reverse transaltion):" << std::endl
                << status_or_ir2.value().DebugString() << std::endl;
      return;
    }

    // Convert IR back to PD.
    PD pd2;
    const auto& status_pd2 = ir_to_pd(info, ir, &pd2);
    if (!status_pd2.ok()) {
      Fail("Reverse translation from IR to PD failed.");
      std::cout << status_pd2.message() << std::endl;
      return;
    }
    if (!diff.Compare(pd, pd2)) {
      Fail("Reverse translation from IR to PD resulted in a different PD.");
      std::cout << "Differences: " << explanation << std::endl;
      std::cout << "PD (after reverse transaltion):" << std::endl
                << pd2.DebugString() << std::endl;
      return;
    }
  }
  std::cout << std::endl;
}

#endif  // P4_PDPI_TESTING_TEST_HELPER_H_
