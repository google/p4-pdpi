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

void RunPiTableEntryTest(const pdpi::IrP4Info& info,
                         const std::string& test_name,
                         const p4::v1::TableEntry& pi) {
  RunGenericPiTest<pdpi::IrTableEntry, p4::v1::TableEntry>(
      info, test_name, pi, pdpi::PiTableEntryToIr);
}

void RunPdTableEntryTest(const pdpi::IrP4Info& info,
                         const std::string& test_name,
                         const pdpi::TableEntry& pd, InputValidity validity) {
  RunGenericPdTest<pdpi::TableEntry, pdpi::IrTableEntry, p4::v1::TableEntry>(
      info, test_name, pd, pdpi::PdTableEntryToIr, pdpi::IrTableEntryToPd,
      pdpi::IrTableEntryToPi, pdpi::PiTableEntryToIr, validity);
}

int main(int argc, char** argv) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
  CHECK(runfiles != nullptr);

  gutil::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(
      GetP4Info(runfiles.get(), "p4_pdpi/testing/main-p4info.pb.txt"));
  CHECK_OK(status_or_info.status());
  pdpi::IrP4Info info = status_or_info.value();

  RunPiTableEntryTest(info, "empty PI",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(""));

  RunPiTableEntryTest(info, "invalid table id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 431
                      )PB"));

  RunPiTableEntryTest(info, "missing matches",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                      )PB"));

  RunPiTableEntryTest(info, "invalid match type - expect exact",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          lpm { value: "\xff\x22" prefix_len: 24 }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "invalid match type - expect lpm",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554436
                        match {
                          field_id: 1
                          ternary { value: "\xff\x22" mask: "\xd3\x54\x12" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "invalid match type - expect ternary",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "invalid match field id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 11
                          exact { value: "\xff\x22" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "invalid bytestring value",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 2
                          exact { value: "\xff\x22\x43\x45\x32" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "invalid prefix length",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554436
                        match {
                          field_id: 1
                          lpm { value: "\xff\x22" prefix_len: 40 }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "duplicate match field id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 1
                          exact { value: "\x10\x24\x32\x52" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "lpm value - masked bits set",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554436
                        match {
                          field_id: 1
                          lpm { value: "\x10\x43\x23\x12" prefix_len: 24 }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "ternary value too long",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x42\x12" mask: "\xff" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "ternary value and mask too long",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x42\x12" mask: "\xff\xff" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "ternary value - masked bits set",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x01\x00" mask: "\x00\xff" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "missing action",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "invalid action",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                        action { action_profile_member_id: 12 }
                      )PB"));
  RunPiTableEntryTest(info, "missing action id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                        action { action { action_id: 1 } }
                      )PB"));
  RunPiTableEntryTest(info, "invalid action id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                        action { action { action_id: 16777219 } }
                      )PB"));
  RunPiTableEntryTest(info, "missing action params",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                        action {
                          action {
                            action_id: 16777217
                            params { param_id: 1 value: "\x54" }
                          }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "duplicate action param id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                        action {
                          action {
                            action_id: 16777217
                            params { param_id: 1 value: "\x54" }
                            params { param_id: 1 value: "\x65" }
                          }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "invalid action param id",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554433
                        match {
                          field_id: 1
                          exact { value: "\xff\x22" }
                        }
                        match {
                          field_id: 2
                          exact { value: "\x10\x24\x32\x52" }
                        }
                        action {
                          action {
                            action_id: 16777217
                            params { param_id: 67 value: "\x54" }
                            params { param_id: 2 value: "\x23" }
                          }
                        }
                      )PB"));
  RunPiTableEntryTest(info, "zero lpm prefix length",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554436
                        match {
                          field_id: 1
                          lpm { value: "\x10\x32\x41\x5" prefix_len: 0 }
                        }
                        action { action { action_id: 21257015 } }
                      )PB"));
  RunPiTableEntryTest(info, "zero ternary mask",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x01\x00" mask: "\x00" }
                        }
                        action {
                          action {
                            action_id: 16777219
                            params { param_id: 1 value: "\x54" }
                            params { param_id: 2 value: "\x23" }
                          }
                        }
                      )PB"));

  RunPdTableEntryTest(info, "empty PD",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(""),
                      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "missing matches",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table {}
                      )PB"),
                      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "missing action", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        id_test_table { match { ipv6: "::ff22" ipv4: "16.36.50.82" } }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "exact match missing",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table {
                          match { ipv6: "::ff22" }
                          action {
                            action2 {
                              normal: "0x54"
                              ipv4: "10.43.12.5"
                              ipv6: "3242::fee2"
                              mac: "00:11:22:33:44:55"
                              str: "hello"
                            }
                          }
                        }
                      )PB"),
                      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "negative prefix length",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: -4 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "prefix length too large",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: 132 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "zero prefix length", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: 0 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "ternary entry with zero mask",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table {
          match {
            normal { value: "0x52" mask: "0x00" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          action { action3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "lpm value - masked bits set",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table {
          match { ipv6 { value: "ffff::abcd:0:aabb" prefix_length: 96 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "ternary value - masked bits set",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table {
          match {
            normal { value: "0x52" mask: "0x01" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          action { action3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "action with all formats as arguments",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table {
                          match { ipv6: "::ff22" ipv4: "16.36.50.82" }
                          action {
                            action2 {
                              normal: "0x54"
                              ipv4: "10.43.12.5"
                              ipv6: "3242::fee2"
                              mac: "00:11:22:33:44:55"
                              str: "hello"
                            }
                          }
                        }
                      )PB"),
                      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "action with missing arguments",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        id_test_table {
          match { ipv6: "::ff22" ipv4: "16.36.50.82" }
          action {
            action2 { normal: "0x54" mac: "00:11:22:33:44:55" str: "hello" }
          }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "action with wrong argument format",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table {
                          match { ipv6: "::ff22" ipv4: "16.36.50.82" }
                          action {
                            action2 {
                              normal: "10.23.43.1"
                              ipv4: "10.43.12.5"
                              ipv6: "3242::fee2"
                              mac: "00:11:22:33:44:55"
                              str: "hello"
                            }
                          }
                        }
                      )PB"),
                      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "exact matches of all formats",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        exact_table {
                          match {
                            normal: "0x54"
                            ipv4: "10.43.12.5"
                            ipv6: "3242::fee2"
                            mac: "00:11:22:33:44:55"
                            str: "hello"
                          }
                          action { NoAction {} }
                        }
                      )PB"),
                      INPUT_IS_VALID);

  RunPdTableEntryTest(info, "ternary with wildcard",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        ternary_table {
                          match { normal { value: "0x52" mask: "0x0273" } }
                          action { action3 { arg1: "0x23" arg2: "0x0251" } }
                        }
                      )PB"),
                      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "ternary table for all formats",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table {
          match {
            normal { value: "0x52" mask: "0x0273" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          action { action3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "ipv4 LPM table", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm1_table {
          match { ipv4 { value: "10.43.12.0" prefix_length: 24 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "ipv6 LPM table", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: 96 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_VALID);

  // TODO(atmanm): Add tests for wcmp and priority
  return 0;
}
