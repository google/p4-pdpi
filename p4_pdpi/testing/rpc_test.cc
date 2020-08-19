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

#include "absl/strings/str_join.h"
#include "gutil/status.h"
#include "gutil/testing.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/pd.h"
#include "p4_pdpi/testing/main_p4_pd.pb.h"
#include "p4_pdpi/testing/test_helper.h"
#include "tools/cpp/runfiles/runfiles.h"

using ::bazel::tools::cpp::runfiles::Runfiles;
using ::p4::config::v1::P4Info;

void RunPiReadRequestTest(const pdpi::IrP4Info& info,
                          const std::string& test_name,
                          const p4::v1::ReadRequest& pi) {
  RunGenericPiTest<pdpi::IrReadRequest, p4::v1::ReadRequest>(
      info, absl::StrCat("ReadRequest test: ", test_name), pi,
      pdpi::PiReadRequestToIr);
}

void RunPdReadRequestTest(const pdpi::IrP4Info& info,
                          const std::string& test_name,
                          const pdpi::ReadRequest& pd,
                          const InputValidity validity) {
  RunGenericPdTest<pdpi::ReadRequest, pdpi::IrReadRequest, p4::v1::ReadRequest>(
      info, absl::StrCat("ReadRequest test: ", test_name), pd,
      pdpi::PdReadRequestToIr, pdpi::IrReadRequestToPd, pdpi::IrReadRequestToPi,
      pdpi::PiReadRequestToIr, validity);
}

void RunPiReadResponseTest(const pdpi::IrP4Info& info,
                           const std::string& test_name,
                           const p4::v1::ReadResponse& pi) {
  RunGenericPiTest<pdpi::IrReadResponse, p4::v1::ReadResponse>(
      info, absl::StrCat("ReadResponse test: ", test_name), pi,
      pdpi::PiReadResponseToIr);
}

void RunPdReadResponseTest(const pdpi::IrP4Info& info,
                           const std::string& test_name,
                           const pdpi::ReadResponse& pd,
                           const InputValidity validity) {
  RunGenericPdTest<pdpi::ReadResponse, pdpi::IrReadResponse,
                   p4::v1::ReadResponse>(
      info, absl::StrCat("ReadResponse test: ", test_name), pd,
      pdpi::PdReadResponseToIr, pdpi::IrReadResponseToPd,
      pdpi::IrReadResponseToPi, pdpi::PiReadResponseToIr, validity);
}

void RunPiUpdateTest(const pdpi::IrP4Info& info, const std::string& test_name,
                     const p4::v1::Update& pi) {
  RunGenericPiTest<pdpi::IrUpdate, p4::v1::Update>(
      info, absl::StrCat("Update test: ", test_name), pi, pdpi::PiUpdateToIr);
}

void RunPdUpdateTest(const pdpi::IrP4Info& info, const std::string& test_name,
                     const pdpi::Update& pd, const InputValidity validity) {
  RunGenericPdTest<pdpi::Update, pdpi::IrUpdate, p4::v1::Update>(
      info, absl::StrCat("Update test: ", test_name), pd, pdpi::PdUpdateToIr,
      pdpi::IrUpdateToPd, pdpi::IrUpdateToPi, pdpi::PiUpdateToIr, validity);
}

void RunPiWriteRequestTest(const pdpi::IrP4Info& info,
                           const std::string& test_name,
                           const p4::v1::WriteRequest& pi) {
  RunGenericPiTest<pdpi::IrWriteRequest, p4::v1::WriteRequest>(
      info, absl::StrCat("WriteRequest test: ", test_name), pi,
      pdpi::PiWriteRequestToIr);
}

void RunPdWriteRequestTest(const pdpi::IrP4Info& info,
                           const std::string& test_name,
                           const pdpi::WriteRequest& pd,
                           const InputValidity validity) {
  RunGenericPdTest<pdpi::WriteRequest, pdpi::IrWriteRequest,
                   p4::v1::WriteRequest>(
      info, absl::StrCat("WriteRequest test: ", test_name), pd,
      pdpi::PdWriteRequestToIr, pdpi::IrWriteRequestToPd,
      pdpi::IrWriteRequestToPi, pdpi::PiWriteRequestToIr, validity);
}

void RunReadRequestTests(pdpi::IrP4Info info) {
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

void RunReadResponseTests(pdpi::IrP4Info info) {
  RunPiReadResponseTest(info, "wrong entity",
                        gutil::ParseProtoOrDie<p4::v1::ReadResponse>(R"PB(
                          entities { action_profile_member {} }
                        )PB"));

  // Invalid IR read responses are tested in table_entry_test.cc, so no
  // RunIrReadResponseTest is needed.

  RunPdReadResponseTest(info, "valid ternary table",
                        gutil::ParseProtoOrDie<pdpi::ReadResponse>(R"PB(
                          table_entries {
                            ternary_table {
                              match { normal { value: "0x52" mask: "0x0273" } }
                              priority: 32
                              action { action3 { arg1: "0x23" arg2: "0x0251" } }
                            }
                          }
                        )PB"),
                        INPUT_IS_VALID);

  RunPdReadResponseTest(info, "multiple tables",
                        gutil::ParseProtoOrDie<pdpi::ReadResponse>(R"PB(
                          table_entries {
                            ternary_table {
                              match { normal { value: "0x52" mask: "0x0273" } }
                              priority: 32
                              action { action3 { arg1: "0x23" arg2: "0x0251" } }
                            }
                          }
                          table_entries {
                            ternary_table {
                              match { normal { value: "0x52" mask: "0x0273" } }
                              priority: 32
                              action { action3 { arg1: "0x23" arg2: "0x0251" } }
                            }
                          }
                        )PB"),
                        INPUT_IS_VALID);
}

void RunUpdateTests(pdpi::IrP4Info info) {
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
                      ternary_table {
                        match { normal { value: "0x52" mask: "0x0273" } }
                        priority: 32
                        action { action3 { arg1: "0x23" arg2: "0x0251" } }
                      }
                    }
                  )PB"),
                  INPUT_IS_INVALID);

  RunPdUpdateTest(info, "valid ternary table",
                  gutil::ParseProtoOrDie<pdpi::Update>(R"PB(
                    type: MODIFY
                    table_entry {
                      ternary_table {
                        match { normal { value: "0x52" mask: "0x0273" } }
                        priority: 32
                        action { action3 { arg1: "0x23" arg2: "0x0251" } }
                      }
                    }
                  )PB"),
                  INPUT_IS_VALID);
}

void RunWriteRequestTests(pdpi::IrP4Info info) {
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
            ternary_table {
              match { normal { value: "0x52" mask: "0x0273" } }
              priority: 32
              action { action3 { arg1: "0x23" arg2: "0x0251" } }
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
            ternary_table {
              match { normal { value: "0x52" mask: "0x0273" } }
              priority: 32
              action { action3 { arg1: "0x23" arg2: "0x0251" } }
            }
          }
        }
        updates {
          type: DELETE
          table_entry {
            ternary_table {
              match { normal { value: "0x52" mask: "0x0273" } }
              priority: 32
              action { action3 { arg1: "0x23" arg2: "0x0251" } }
            }
          }
        }
      )PB"),
      INPUT_IS_VALID);
}

int main(int argc, char** argv) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
  CHECK(runfiles != nullptr);

  gutil::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(
      GetP4Info(runfiles.get(), "p4_pdpi/testing/main-p4info.pb.txt"));
  CHECK_OK(status_or_info.status());
  pdpi::IrP4Info info = status_or_info.value();

  RunReadRequestTests(info);
  RunReadResponseTests(info);
  RunUpdateTests(info);
  RunWriteRequestTests(info);

  return 0;
}
