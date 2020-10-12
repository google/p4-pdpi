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

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "glog/logging.h"
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

static void RunPiPacketInTest(const pdpi::IrP4Info& info,
                              const std::string& test_name,
                              const p4::v1::PacketIn& pi) {
  RunGenericPiTest<pdpi::IrPacketIn, p4::v1::PacketIn>(
      info, absl::StrCat("PacketIn test: ", test_name), pi,
      pdpi::PiPacketInToIr);
}

static void RunPdPacketInTest(const pdpi::IrP4Info& info,
                              const std::string& test_name,
                              const pdpi::PacketIn& pd,
                              const InputValidity validity) {
  RunGenericPdTest<pdpi::PacketIn, pdpi::IrPacketIn, p4::v1::PacketIn>(
      info, absl::StrCat("PacketIn test: ", test_name), pd,
      pdpi::PdPacketInToIr, pdpi::IrPacketInToPd, pdpi::IrPacketInToPi,
      pdpi::PiPacketInToIr, validity);
}

static void RunPiPacketOutTest(const pdpi::IrP4Info& info,
                               const std::string& test_name,
                               const p4::v1::PacketOut& pi) {
  RunGenericPiTest<pdpi::IrPacketOut, p4::v1::PacketOut>(
      info, absl::StrCat("PacketOut test: ", test_name), pi,
      pdpi::PiPacketOutToIr);
}

static void RunPdPacketOutTest(const pdpi::IrP4Info& info,
                               const std::string& test_name,
                               const pdpi::PacketOut& pd,
                               const InputValidity validity) {
  RunGenericPdTest<pdpi::PacketOut, pdpi::IrPacketOut, p4::v1::PacketOut>(
      info, absl::StrCat("PacketOut test: ", test_name), pd,
      pdpi::PdPacketOutToIr, pdpi::IrPacketOutToPd, pdpi::IrPacketOutToPi,
      pdpi::PiPacketOutToIr, validity);
}

static void RunPacketInTests(pdpi::IrP4Info info) {
  RunPiPacketInTest(info, "duplicate id",
                    gutil::ParseProtoOrDie<p4::v1::PacketIn>(R"PB(
                      payload: "1"
                      metadata { metadata_id: 1 value: "\x34" }
                      metadata { metadata_id: 1 value: "\x34" }
                    )PB"));

  RunPiPacketInTest(info, "extra metadata",
                    gutil::ParseProtoOrDie<p4::v1::PacketIn>(R"PB(
                      payload: "1"
                      metadata { metadata_id: 1 value: "\x34" }
                      metadata { metadata_id: 2 value: "\x23" }
                      metadata { metadata_id: 3 value: "\x124" }
                    )PB"));
  RunPiPacketInTest(info, "missing metadata",
                    gutil::ParseProtoOrDie<p4::v1::PacketIn>(R"PB(
                      payload: "1"
                      metadata { metadata_id: 1 value: "\x34" }
                    )PB"));
  RunPdPacketInTest(
      info, "ok", gutil::ParseProtoOrDie<pdpi::PacketIn>(R"PB(
        payload: "1"
        metadata { ingress_port: "0x34" target_egress_port: "eth-1/2/3" }
      )PB"),
      INPUT_IS_VALID);
}

static void RunPacketOutTests(pdpi::IrP4Info info) {
  RunPiPacketOutTest(info, "duplicate id",
                     gutil::ParseProtoOrDie<p4::v1::PacketOut>(R"PB(
                       payload: "1"
                       metadata { metadata_id: 1 value: "\x1" }
                       metadata { metadata_id: 1 value: "\x1" }
                     )PB"));

  RunPiPacketOutTest(info, "missing metadata",
                     gutil::ParseProtoOrDie<p4::v1::PacketOut>(R"PB(
                       payload: "1"
                       metadata { metadata_id: 1 value: "\x1" }
                     )PB"));
  RunPiPacketOutTest(info, "extra metadata",
                     gutil::ParseProtoOrDie<p4::v1::PacketOut>(R"PB(
                       payload: "1"
                       metadata { metadata_id: 1 value: "\x0" }
                       metadata { metadata_id: 2 value: "\x1" }
                       metadata { metadata_id: 3 value: "\x1" }
                     )PB"));
  RunPdPacketOutTest(
      info, "ok", gutil::ParseProtoOrDie<pdpi::PacketOut>(R"PB(
        payload: "1"
        metadata { submit_to_ingress: "0x1" egress_port: "eth-1/2/3" }
      )PB"),
      INPUT_IS_VALID);
}

int main(int argc, char** argv) {
  CHECK(argc == 2);  // Usage: packet_io_test <p4info file>.
  const auto p4info =
      gutil::ParseProtoFileOrDie<p4::config::v1::P4Info>(argv[1]);

  absl::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(p4info);
  CHECK_OK(status_or_info.status());
  pdpi::IrP4Info info = status_or_info.value();

  RunPacketInTests(info);
  RunPacketOutTests(info);
  return 0;
}
