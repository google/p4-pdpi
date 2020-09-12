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

#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "glog/logging.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "grpcpp/grpcpp.h"
#include "gutil/status.h"
#include "gutil/testing.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/pd.h"
#include "p4_pdpi/testing/main_p4_pd.pb.h"
#include "p4_pdpi/testing/test_helper.h"

using ::p4::config::v1::P4Info;

// Overload the StatusOr insertion operator to output its status. This allows
// StatusOr to be directly used in LOG messages.
template <typename T>
std::ostream& operator<<(std::ostream& out,
                         const absl::StatusOr<T>& status_or) {
  return out << status_or.status();
}

static void RunPiReadRequestTest(const pdpi::IrP4Info& info,
                                 const std::string& test_name,
                                 const p4::v1::ReadRequest& pi) {
  RunGenericPiTest<pdpi::IrReadRequest, p4::v1::ReadRequest>(
      info, absl::StrCat("ReadRequest test: ", test_name), pi,
      pdpi::PiReadRequestToIr);
}

static void RunPdReadRequestTest(const pdpi::IrP4Info& info,
                                 const std::string& test_name,
                                 const pdpi::ReadRequest& pd,
                                 const InputValidity validity) {
  RunGenericPdTest<pdpi::ReadRequest, pdpi::IrReadRequest, p4::v1::ReadRequest>(
      info, absl::StrCat("ReadRequest test: ", test_name), pd,
      pdpi::PdReadRequestToIr, pdpi::IrReadRequestToPd, pdpi::IrReadRequestToPi,
      pdpi::PiReadRequestToIr, validity);
}

static void RunPiReadResponseTest(const pdpi::IrP4Info& info,
                                  const std::string& test_name,
                                  const p4::v1::ReadResponse& pi) {
  RunGenericPiTest<pdpi::IrReadResponse, p4::v1::ReadResponse>(
      info, absl::StrCat("ReadResponse test: ", test_name), pi,
      pdpi::PiReadResponseToIr);
}

static void RunPdReadResponseTest(const pdpi::IrP4Info& info,
                                  const std::string& test_name,
                                  const pdpi::ReadResponse& pd,
                                  const InputValidity validity) {
  RunGenericPdTest<pdpi::ReadResponse, pdpi::IrReadResponse,
                   p4::v1::ReadResponse>(
      info, absl::StrCat("ReadResponse test: ", test_name), pd,
      pdpi::PdReadResponseToIr, pdpi::IrReadResponseToPd,
      pdpi::IrReadResponseToPi, pdpi::PiReadResponseToIr, validity);
}

static void RunPiUpdateTest(const pdpi::IrP4Info& info,
                            const std::string& test_name,
                            const p4::v1::Update& pi) {
  RunGenericPiTest<pdpi::IrUpdate, p4::v1::Update>(
      info, absl::StrCat("Update test: ", test_name), pi, pdpi::PiUpdateToIr);
}

static void RunPdUpdateTest(const pdpi::IrP4Info& info,
                            const std::string& test_name,
                            const pdpi::Update& pd,
                            const InputValidity validity) {
  RunGenericPdTest<pdpi::Update, pdpi::IrUpdate, p4::v1::Update>(
      info, absl::StrCat("Update test: ", test_name), pd, pdpi::PdUpdateToIr,
      pdpi::IrUpdateToPd, pdpi::IrUpdateToPi, pdpi::PiUpdateToIr, validity);
}

static void RunPiWriteRequestTest(const pdpi::IrP4Info& info,
                                  const std::string& test_name,
                                  const p4::v1::WriteRequest& pi) {
  RunGenericPiTest<pdpi::IrWriteRequest, p4::v1::WriteRequest>(
      info, absl::StrCat("WriteRequest test: ", test_name), pi,
      pdpi::PiWriteRequestToIr);
}

static void RunPdWriteRequestTest(const pdpi::IrP4Info& info,
                                  const std::string& test_name,
                                  const pdpi::WriteRequest& pd,
                                  const InputValidity validity) {
  RunGenericPdTest<pdpi::WriteRequest, pdpi::IrWriteRequest,
                   p4::v1::WriteRequest>(
      info, absl::StrCat("WriteRequest test: ", test_name), pd,
      pdpi::PdWriteRequestToIr, pdpi::IrWriteRequestToPd,
      pdpi::IrWriteRequestToPi, pdpi::PiWriteRequestToIr, validity);
}

static void RunInvalidGrpcFailToTranslateToIrTest(
    const std::string& test_name, int number_of_write_request,
    const grpc::Status& grpc_status) {
  std::cout << TestHeader(absl::StrCat(
                   "Invalid gRPC WriteRpcStatus should fail test: ", test_name))
            << std::endl
            << std::endl;
  std::cout << "--- gRPC (Input):" << std::endl;
  std::cout << pdpi::WriteRequestGrpcStatusToString(grpc_status);

  // Grpc -> Absl
  std::cout << "--- absl::Status:" << std::endl;
  std::cout << pdpi::GrpcStatusToAbslStatus(grpc_status,
                                            number_of_write_request)
            << std::endl;

  // Grpc -> IR
  const auto& status_or_ir =
      pdpi::GrpcStatusToIrWriteRpcStatus(grpc_status, number_of_write_request);
  if (!status_or_ir.ok()) {
    std::cout << "--- gRPC is invalid/unsupported:" << std::endl;
    std::cout << status_or_ir.status() << std::endl << std::endl;
  } else {
    Fail("Expected gRPC status to be invalid.");
  }
}

static void RunInvalidIrFailToTranslateToGrpcTest(
    const std::string& test_name,
    const pdpi::IrWriteRpcStatus& ir_write_rpc_status) {
  std::cout << TestHeader(absl::StrCat(
                   "Invalid Ir WriteRpcStatus should fail test: ", test_name))
            << std::endl
            << std::endl;
  std::cout << "--- IR (Input):" << std::endl;
  std::cout << ir_write_rpc_status.DebugString();
  const auto& status_or_grpc =
      pdpi::IrWriteRpcStatusToGrpcStatus(ir_write_rpc_status);
  if (!status_or_grpc.ok()) {
    std::cout << "--- IR is invalid/unsupported:" << std::endl
              << status_or_grpc << std::endl
              << std::endl;
  } else {
    Fail("Expected IR to be invalid.");
  }
}

// Runs PD -> IR -> Grpc -> IR2 -> PD2 and if validity == INPUT_IS_VALID, checks
// IR == IR2 and  PD == PD2
static void RunPdWriteRpcStatusTest(const std::string& test_name,
                                    const pdpi::WriteRpcStatus& pd,
                                    int number_of_update_status,
                                    InputValidity validity) {
  if (validity == INPUT_IS_VALID) {
    std::cout << TestHeader(
        absl::StrCat("Pd WriteRpcStatus test (INPUT_IS_VALID): ", test_name));
  } else {
    std::cout << TestHeader(
        absl::StrCat("Pd WriteRpcStatus test (INPUT_IS_INVALID): ", test_name));
  }
  std::cout << std::endl << std::endl;
  std::cout << "--- PD(input):" << std::endl;
  std::cout << pd.DebugString() << std::endl;
  // Set-up message differencer.
  google::protobuf::util::MessageDifferencer diff;
  diff.set_report_moves(false);
  diff.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::RepeatedFieldComparison::
          AS_SET);
  std::string explanation;
  diff.ReportDifferencesToString(&explanation);

  // PD -> IR
  const auto& status_or_ir = pdpi::PdWriteRpcStatusToIr(pd);
  if (!status_or_ir.ok()) {
    if (validity == INPUT_IS_VALID) {
      Fail(
          "Translation from PD to IR failed even though input was marked "
          "valid.");
      std::cout << status_or_ir.status().message() << std::endl;
      return;
    } else {
      std::cout << "---PD is invalid/unsupported:" << std::endl;
      std::cout << status_or_ir.status() << std::endl << std::endl << std::endl;
      return;
    }
  }
  const auto& ir = status_or_ir.value();
  std::cout << "---IR:" << std::endl;
  std::cout << ir.DebugString() << std::endl;

  // IR -> Grpc
  const auto& status_or_grpc_status = pdpi::IrWriteRpcStatusToGrpcStatus(ir);
  if (!status_or_grpc_status.ok()) {
    if (validity == INPUT_IS_VALID) {
      Fail(
          "Translation from IR to gRPC failed even though input was marked "
          "valid.");
      std::cout << status_or_grpc_status.status().message() << std::endl;
      return;
    } else {
      std::cout << "---PD is invalid/unsupported (detected when translating IR "
                   "to gRPC.)\n";
      std::cout << status_or_grpc_status.status().message() << std::endl
                << std::endl
                << std::endl;
      return;
    }
  }
  if (validity == INPUT_IS_INVALID) {
    Fail(
        "PD was marked invalid but translation from PD to IR and IR to gRPC "
        "both succeeded.");
    return;
  }

  // At this point, validity == INPUT_IS_VALID
  const auto& grpc_write_status = status_or_grpc_status.value();
  std::cout << "---gRPC Status:" << std::endl;
  std::cout << pdpi::WriteRequestGrpcStatusToString(grpc_write_status)
            << std::endl;

  // Grpc -> Absl
  std::cout << "--- absl::Status:" << std::endl;
  std::cout << pdpi::GrpcStatusToAbslStatus(grpc_write_status,
                                            number_of_update_status)
            << std::endl;

  // Grpc -> IR2
  const auto& status_or_ir2 = pdpi::GrpcStatusToIrWriteRpcStatus(
      grpc_write_status, number_of_update_status);
  if (!status_or_ir2.ok()) {
    Fail("Translation from gRPC to IR failed");
    std::cout << status_or_ir2.status().message() << std::endl;
    return;
  }
  const auto& ir2 = status_or_ir2.value();
  if (!diff.Compare(ir, ir2)) {
    Fail("Reverse translation from gRPC to IR resulted in a different IR.");
    std::cout << "Differences: " << explanation << std::endl;
    std::cout << "IR(after reverse translation):" << std::endl
              << ir2.DebugString() << std::endl;
    return;
  }

  // IR2 -> PD2
  pdpi::WriteRpcStatus pd2;
  const auto pd2_translation_status = pdpi::IrWriteRpcStatusToPd(ir, &pd2);

  if (!pd2_translation_status.ok()) {
    Fail("Translation from IR2 to PD2 failed");
    std::cout << pd2_translation_status.message() << std::endl;
    return;
  }
  if (!diff.Compare(pd, pd2)) {
    Fail("Reverse translation from IR2 to PD2 resulted in a different PD");
    std::cout << "Differences: " << explanation << std::endl;
    std::cout << "PD(after reverse translation):" << std::endl
              << pd2.DebugString() << std::endl;
    return;
  }
  std::cout << std::endl;
}

static void RunReadRequestTests(pdpi::IrP4Info info) {
  RunPiReadRequestTest(info, "empty",
                       gutil::ParseProtoOrDie<p4::v1::ReadRequest>(""));

  RunPiReadRequestTest(info, "no entities",
                       gutil::ParseProtoOrDie<p4::v1::ReadRequest>(R"PB(
                         device_id: 10
                       )PB"));

  RunPiReadRequestTest(info, "wrong entities",
                       gutil::ParseProtoOrDie<p4::v1::ReadRequest>(R"PB(
                         device_id: 10
                         entities { action_profile_member {} }
                       )PB"));

  RunPiReadRequestTest(info, "multiple table entries",
                       gutil::ParseProtoOrDie<p4::v1::ReadRequest>(R"PB(
                         device_id: 10
                         entities { table_entry {} }
                         entities { table_entry {} }
                       )PB"));

  // There are no invalid IR read requests, so no RunIrReadRequestTest is
  // needed.

  RunPdReadRequestTest(info, "no meter, no counter",
                       gutil::ParseProtoOrDie<pdpi::ReadRequest>(R"PB(
                         device_id: 10
                       )PB"),
                       INPUT_IS_VALID);
  RunPdReadRequestTest(info, "meter, no counter",
                       gutil::ParseProtoOrDie<pdpi::ReadRequest>(R"PB(
                         device_id: 10
                         read_meter_configs: true
                       )PB"),
                       INPUT_IS_VALID);
  RunPdReadRequestTest(info, "no meter, counter",
                       gutil::ParseProtoOrDie<pdpi::ReadRequest>(R"PB(
                         device_id: 10
                         read_counter_data: true
                       )PB"),
                       INPUT_IS_VALID);
}

static void RunReadResponseTests(pdpi::IrP4Info info) {
  RunPiReadResponseTest(info, "wrong entity",
                        gutil::ParseProtoOrDie<p4::v1::ReadResponse>(R"PB(
                          entities { action_profile_member {} }
                        )PB"));

  // Invalid IR read responses are tested in table_entry_test.cc, so no
  // RunIrReadResponseTest is needed.

  RunPdReadResponseTest(
      info, "valid ternary table",
      gutil::ParseProtoOrDie<pdpi::ReadResponse>(R"PB(
        table_entries {
          ternary_table_entry {
            match { normal { value: "0x52" mask: "0x273" } }
            priority: 32
            action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
          }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdReadResponseTest(
      info, "multiple tables", gutil::ParseProtoOrDie<pdpi::ReadResponse>(R"PB(
        table_entries {
          ternary_table_entry {
            match { normal { value: "0x52" mask: "0x273" } }
            priority: 32
            action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
          }
        }
        table_entries {
          ternary_table_entry {
            match { normal { value: "0x52" mask: "0x273" } }
            priority: 32
            action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
          }
        }
      )PB"),
      INPUT_IS_VALID);
}

static void RunUpdateTests(pdpi::IrP4Info info) {
  RunPiUpdateTest(info, "empty", gutil::ParseProtoOrDie<p4::v1::Update>(R"PB(
                  )PB"));

  RunPiUpdateTest(info, "missing type",
                  gutil::ParseProtoOrDie<p4::v1::Update>(R"PB(
                    entity { table_entry {} }
                  )PB"));

  RunPiUpdateTest(info, "wrong entity",
                  gutil::ParseProtoOrDie<p4::v1::Update>(R"PB(
                    type: INSERT
                    entity { action_profile_member {} }
                  )PB"));

  // Invalid IR update table_entries are tested in table_entry_test.cc and
  // invalid type is tested in PD tests. No RunIrUpdateTest is needed.

  RunPdUpdateTest(info, "missing type",
                  gutil::ParseProtoOrDie<pdpi::Update>(R"PB(
                    table_entry {
                      ternary_table_entry {
                        match { normal { value: "0x52" mask: "0x273" } }
                        priority: 32
                        action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
                      }
                    }
                  )PB"),
                  INPUT_IS_INVALID);

  RunPdUpdateTest(info, "valid ternary table",
                  gutil::ParseProtoOrDie<pdpi::Update>(R"PB(
                    type: MODIFY
                    table_entry {
                      ternary_table_entry {
                        match { normal { value: "0x52" mask: "0x273" } }
                        priority: 32
                        action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
                      }
                    }
                  )PB"),
                  INPUT_IS_VALID);
}

static void RunWriteRequestTests(pdpi::IrP4Info info) {
  RunPiWriteRequestTest(info, "invalid role_id",
                        gutil::ParseProtoOrDie<p4::v1::WriteRequest>(R"PB(
                          role_id: 1
                        )PB"));

  RunPiWriteRequestTest(info, "invalid atomicity",
                        gutil::ParseProtoOrDie<p4::v1::WriteRequest>(R"PB(
                          role_id: 0
                          atomicity: ROLLBACK_ON_ERROR
                        )PB"));

  // Invalid updates values are tested in RunUpdateTests.
  // Invalid IR WriteRequests are tested in PD tests. No RunIrWriteRequestTest
  // is needed.

  RunPdWriteRequestTest(info, "empty",
                        gutil::ParseProtoOrDie<pdpi::WriteRequest>(R"PB(
                        )PB"),
                        INPUT_IS_VALID);

  RunPdWriteRequestTest(info, "missing updates",
                        gutil::ParseProtoOrDie<pdpi::WriteRequest>(R"PB(
                          device_id: 134
                          election_id { high: 23413 low: 2312 }
                        )PB"),
                        INPUT_IS_VALID);

  RunPdWriteRequestTest(
      info, "valid ternary table update",
      gutil::ParseProtoOrDie<pdpi::WriteRequest>(R"PB(
        device_id: 113
        election_id { high: 1231 low: 77989 }
        updates {
          type: MODIFY
          table_entry {
            ternary_table_entry {
              match { normal { value: "0x52" mask: "0x273" } }
              priority: 32
              action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
            }
          }
        }
      )PB"),
      INPUT_IS_VALID);
  RunPdWriteRequestTest(
      info, "multiple updates", gutil::ParseProtoOrDie<pdpi::WriteRequest>(R"PB(
        device_id: 113
        election_id { high: 1231 low: 77989 }
        updates {
          type: MODIFY
          table_entry {
            ternary_table_entry {
              match { normal { value: "0x52" mask: "0x273" } }
              priority: 32
              action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
            }
          }
        }
        updates {
          type: DELETE
          table_entry {
            ternary_table_entry {
              match { normal { value: "0x52" mask: "0x273" } }
              priority: 32
              action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
            }
          }
        }
      )PB"),
      INPUT_IS_VALID);
}

static google::rpc::Status GenerateGoogleRpcStatus(
    google::rpc::Code status_code, const std::string& message,
    const std::vector<p4::v1::Error>& p4_errors) {
  google::rpc::Status google_rpc_status;
  google_rpc_status.set_code(status_code);
  google_rpc_status.set_message(message);
  for (const auto& p4_error : p4_errors) {
    google_rpc_status.add_details()->PackFrom(p4_error);
  }
  return google_rpc_status;
}

static void RunWriteRpcStatusTest() {
  int number_of_statuses_for_invalid_test = 42;
  RunInvalidGrpcFailToTranslateToIrTest(
      "Grpc status has ok status with non empty message",
      number_of_statuses_for_invalid_test,
      grpc::Status(grpc::StatusCode::OK, "message_string"));
  RunInvalidGrpcFailToTranslateToIrTest(
      "Invalid gRPC StatusCode", number_of_statuses_for_invalid_test,
      grpc::Status(static_cast<grpc::StatusCode>(42), "error_message"));

  // Use p4 errors below to construct google::rpc::status
  p4::v1::Error ok_p4_error =
      gutil::ParseProtoOrDie<p4::v1::Error>(R"PB(canonical_code: 0)PB");
  p4::v1::Error resource_exhausted_p4_error =
      gutil::ParseProtoOrDie<p4::v1::Error>(R"PB(canonical_code: 8
                                                 message: "table is full")PB");
  p4::v1::Error ok_p4_error_message_with_message =
      gutil::ParseProtoOrDie<p4::v1::Error>(R"PB(canonical_code: 0
                                                 message: "some message")PB");
  p4::v1::Error non_ok_p4_error_with_no_message =
      gutil::ParseProtoOrDie<p4::v1::Error>(R"PB(canonical_code: 2)PB");
  p4::v1::Error p4_error_with_invalid_canonical_code =
      gutil::ParseProtoOrDie<p4::v1::Error>(R"PB(canonical_code: 42)PB");

  std::vector<p4::v1::Error> all_ok_p4_errors{ok_p4_error, ok_p4_error,
                                              ok_p4_error};
  grpc::Status all_ok_p4_status_grpc_status(
      grpc::StatusCode::UNKNOWN, "batch update all successful",
      GenerateGoogleRpcStatus(google::rpc::Code::UNKNOWN,
                              "batch update all successful", all_ok_p4_errors)
          .SerializeAsString());
  RunInvalidGrpcFailToTranslateToIrTest(
      "None of p4_error contained in google::rpc::status within grpc::Status "
      "is non-ok",
      all_ok_p4_errors.size(), all_ok_p4_status_grpc_status);

  std::vector<p4::v1::Error> invalid_p4_errors{
      ok_p4_error, resource_exhausted_p4_error,
      ok_p4_error_message_with_message};
  grpc::Status mix_of_success_and_failure_invalid_batch_update_grpc_status(
      grpc::StatusCode::UNKNOWN, "mix of successful and failed batch update",
      GenerateGoogleRpcStatus(google::rpc::Code::UNKNOWN,
                              "mix of successful and failed batch update",
                              invalid_p4_errors)
          .SerializeAsString());
  RunInvalidGrpcFailToTranslateToIrTest(
      "Invalid p4 error has ok status but has non-empty message",
      invalid_p4_errors.size(),
      mix_of_success_and_failure_invalid_batch_update_grpc_status);

  grpc::Status grpc_status_with_inner_status_with_different_message(
      grpc::StatusCode::UNKNOWN, "some message",
      GenerateGoogleRpcStatus(google::rpc::Code::UNKNOWN, "different message",
                              {resource_exhausted_p4_error})
          .SerializeAsString());

  grpc::Status grpc_status_with_inner_status_with_different_code(
      grpc::StatusCode::UNKNOWN, "some message",
      GenerateGoogleRpcStatus(google::rpc::Code::RESOURCE_EXHAUSTED,
                              "some message", {resource_exhausted_p4_error})
          .SerializeAsString());
  RunInvalidGrpcFailToTranslateToIrTest(
      "gRPC status has code that is different from code contained in "
      "google::rpc::Status",
      1, grpc_status_with_inner_status_with_different_code);

  grpc::Status grpc_status_with_mismatching_status_for_batch_update(
      grpc::StatusCode::RESOURCE_EXHAUSTED, "some message",
      GenerateGoogleRpcStatus(google::rpc::Code::RESOURCE_EXHAUSTED,
                              "some message", {resource_exhausted_p4_error})
          .SerializeAsString());
  RunInvalidGrpcFailToTranslateToIrTest(
      "gRPC status contains batch update information but does not have UNKNOWN "
      "status",
      1, grpc_status_with_mismatching_status_for_batch_update);

  grpc::Status grpc_status_with_invalid_p4_error(
      grpc::StatusCode::UNKNOWN, "some message",
      GenerateGoogleRpcStatus(google::rpc::Code::UNKNOWN, "some message",
                              {p4_error_with_invalid_canonical_code})
          .SerializeAsString());
  RunInvalidGrpcFailToTranslateToIrTest(
      "gRPC status has batch update information but p4 error's canonical_code "
      "is not valid",
      1, grpc_status_with_invalid_p4_error);

  RunInvalidIrFailToTranslateToGrpcTest(
      "IR rpc_response has ok code but non empty message",
      gutil::ParseProtoOrDie<pdpi::IrWriteRpcStatus>(R"PB(
        rpc_response: {
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: OK message: "error_message" }
          statuses: { code: OK message: "error_message" }
        }
      )PB"));
  RunInvalidIrFailToTranslateToGrpcTest(
      "IR rpc_response has non ok status code but empty message",
      gutil::ParseProtoOrDie<pdpi::IrWriteRpcStatus>(R"PB(
        rpc_response: {
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: UNKNOWN }
          statuses: { code: UNKNOWN }
        }
      )PB"));
  RunInvalidIrFailToTranslateToGrpcTest(
      "IR rpc_response has status with invalid code",
      gutil::ParseProtoOrDie<pdpi::IrWriteRpcStatus>(R"PB(
        rpc_response: {
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: OK }
          statuses: { code: 42 message: "42 is invalid" }
        }
      )PB"));
  RunInvalidIrFailToTranslateToGrpcTest(
      "IR rpc_wide_error has invalid code",
      gutil::ParseProtoOrDie<pdpi::IrWriteRpcStatus>(R"PB(
        rpc_wide_error: { code: 42 message: "invalid_code" }
      )PB"));
  RunInvalidIrFailToTranslateToGrpcTest(
      "IR rpc_wide_error should not have ok status",
      gutil::ParseProtoOrDie<pdpi::IrWriteRpcStatus>(R"PB(
        rpc_wide_error: { code: 0 message: "ok_code" }
      )PB"));

  RunInvalidIrFailToTranslateToGrpcTest(
      "IR non ok rpc_wide_error should have non-empty message",
      gutil::ParseProtoOrDie<pdpi::IrWriteRpcStatus>(R"PB(
        rpc_wide_error: { code: 2 }
      )PB"));

  RunPdWriteRpcStatusTest("PD rpc_wide error has invalid code",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_wide_error: { code: 42 message: "bad_code" }
                          )PB"),
                          5, INPUT_IS_INVALID);
  RunPdWriteRpcStatusTest("non-ok status with empty message should fail",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_response: {
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK message: "error_message" }
                              statuses: { code: OK message: "error_message" }
                            }
                          )PB"),
                          5, INPUT_IS_INVALID);
  RunPdWriteRpcStatusTest("invalid status in rpc response",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_response: {
                              statuses: { code: 42 }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: UNKNOWN }
                            }
                          )PB"),
                          5, INPUT_IS_INVALID);

  RunPdWriteRpcStatusTest("non-ok status with empty message should fail",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_response: {
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: UNKNOWN }
                            }
                          )PB"),
                          5, INPUT_IS_INVALID);

  // Tests translation of PD with all ok status should success when
  // number_of_update_status matches with the repeated statuses field in PD
  RunPdWriteRpcStatusTest("all reads status ok",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_response: {
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                              statuses: { code: OK }
                            }
                          )PB"),
                          5, INPUT_IS_VALID);
  // RPC-wide error tests
  RunPdWriteRpcStatusTest("rpc-wide error with ok status code",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_wide_error: { code: 0 message: "code is ok" }
                          )PB"),
                          5, INPUT_IS_INVALID);
  RunPdWriteRpcStatusTest("rpc-wide error with invalid status code",
                          gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
                            rpc_wide_error: { code: 42 message: "bad_code" }
                          )PB"),
                          5, INPUT_IS_INVALID);
  RunPdWriteRpcStatusTest(
      "rpc-wide error with ABORTED status",
      gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
        rpc_wide_error: { code: 10 message: "int value of ABORTED is 10" }
      )PB"),
      5, INPUT_IS_VALID);

  // Mix of successful and failed batch write update.
  // This is the same error status in p4RT spec example.
  RunPdWriteRpcStatusTest(
      "mix of successful and failed write update",
      gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
        rpc_response: {
          statuses: { code: 8 message: "Table is full." }
          statuses: { code: 0 }
          statuses: { code: 6 message: "Entity already exists." }

        }
      )PB"),
      3, INPUT_IS_VALID);

  RunPdWriteRpcStatusTest(
      "all write failed", gutil::ParseProtoOrDie<pdpi::WriteRpcStatus>(R"PB(
        rpc_response: {
          statuses: { code: RESOURCE_EXHAUSTED message: "Table is full." }
          statuses: {
            code: INVALID_ARGUMENT
            message: "can not parse write request."
          }
          statuses: { code: ALREADY_EXISTS message: "entry already exists." }
        }
      )PB"),
      3, INPUT_IS_VALID);
}

int main(int argc, char** argv) {
  CHECK(argc == 2);  // Usage: rpc_test <p4info file>.
  const auto p4info =
      gutil::ParseProtoFileOrDie<p4::config::v1::P4Info>(argv[1]);

  absl::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(p4info);
  CHECK_OK(status_or_info.status()) << status_or_info.status();
  pdpi::IrP4Info info = status_or_info.value();

  RunReadRequestTests(info);
  RunReadResponseTests(info);
  RunUpdateTests(info);
  RunWriteRequestTests(info);
  RunWriteRpcStatusTest();
  return 0;
}
