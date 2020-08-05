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

#include <google/protobuf/text_format.h>

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "gutil/proto.h"
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
                          const pdpi::ReadRequest& pd) {
  RunGenericPdTest<pdpi::ReadRequest, pdpi::IrReadRequest, p4::v1::ReadRequest>(
      info, absl::StrCat("ReadRequest test: ", test_name), pd,
      pdpi::PdReadRequestToIr, pdpi::IrReadRequestToPd, pdpi::IrReadRequestToPi,
      pdpi::PiReadRequestToIr);
}

int main(int argc, char** argv) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
  CHECK(runfiles != nullptr);

  gutil::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(
      GetP4Info(runfiles.get(), "p4_pdpi/testing/main-p4info.pb.txt"));
  CHECK_OK(status_or_info.status());
  pdpi::IrP4Info info = status_or_info.value();

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
                       )PB"));
  RunPdReadRequestTest(info, "meter, no counter",
                       gutil::ParseProtoOrDie<pdpi::ReadRequest>(R"PB(
                         device_id: 10
                         read_meter_configs: true
                       )PB"));
  RunPdReadRequestTest(info, "no meter, counter",
                       gutil::ParseProtoOrDie<pdpi::ReadRequest>(R"PB(
                         device_id: 10
                         read_counter_data: true
                       )PB"));

  return 0;
}
