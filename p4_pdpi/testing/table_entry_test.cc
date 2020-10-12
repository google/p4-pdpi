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

static void RunPiTableEntryTest(const pdpi::IrP4Info& info,
                                const std::string& test_name,
                                const p4::v1::TableEntry& pi) {
  RunGenericPiTest<pdpi::IrTableEntry, p4::v1::TableEntry>(
      info, test_name, pi, pdpi::PiTableEntryToIr);
}

static void RunIrTableEntryTest(const pdpi::IrP4Info& info,
                                const std::string& test_name,
                                const pdpi::IrTableEntry& ir) {
  RunGenericIrTest<pdpi::IrTableEntry, p4::v1::TableEntry>(
      info, test_name, ir, pdpi::IrTableEntryToPi);
}

static void RunPdTableEntryTest(const pdpi::IrP4Info& info,
                                const std::string& test_name,
                                const pdpi::TableEntry& pd,
                                InputValidity validity) {
  RunGenericPdTest<pdpi::TableEntry, pdpi::IrTableEntry, p4::v1::TableEntry>(
      info, test_name, pd, pdpi::PdTableEntryToIr, pdpi::IrTableEntryToPd,
      pdpi::IrTableEntryToPi, pdpi::PiTableEntryToIr, validity);
}

static void RunPiTests(const pdpi::IrP4Info info) {
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
                        priority: 32
                      )PB"));

  RunPiTableEntryTest(info, "invalid match type - expect optional",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554441
                        match {
                          field_id: 1
                          lpm { value: "\xff\x22" prefix_len: 24 }
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
                        priority: 32
                      )PB"));

  RunPiTableEntryTest(info, "ternary value and mask too long",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x42\x12" mask: "\xff\xff" }
                        }
                        priority: 32
                      )PB"));

  RunPiTableEntryTest(info, "ternary value - masked bits set",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x01\x00" mask: "\x00\xff" }
                        }
                        priority: 32
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

  RunPiTableEntryTest(info, "action set in table with action",
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
                          action_profile_action_set {
                            action_profile_actions {
                              action {
                                action_id: 16777217
                                params { param_id: 1 value: "\000\000\000\010" }
                                params { param_id: 2 value: "\000\000\000\011" }
                              }
                              weight: 1
                            }
                          }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "action in table with action set",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554438
                        match {
                          field_id: 1
                          lpm { value: "\xff\x00" prefix_len: 24 }
                        }
                        action {
                          action {
                            action_id: 16777217
                            params { param_id: 1 value: "\000\000\000\010" }
                            params { param_id: 2 value: "\000\000\000\011" }
                          }
                        }
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
                        priority: 32
                        action {
                          action {
                            action_id: 16777219
                            params { param_id: 1 value: "\x54" }
                            params { param_id: 2 value: "\x23" }
                          }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "zero priority",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x01\x00" mask: "\x01\xff" }
                        }
                        priority: 0
                        action {
                          action {
                            action_id: 16777219
                            params { param_id: 1 value: "\x54" }
                            params { param_id: 2 value: "\x23" }
                          }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "negative priority",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x01\x00" mask: "\x01\xff" }
                        }
                        priority: -32
                        action {
                          action {
                            action_id: 16777219
                            params { param_id: 1 value: "\x54" }
                            params { param_id: 2 value: "\x23" }
                          }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "absent priority",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554435
                        match {
                          field_id: 1
                          ternary { value: "\x01\x00" mask: "\x01\xff" }
                        }
                        action {
                          action {
                            action_id: 16777219
                            params { param_id: 1 value: "\x54" }
                            params { param_id: 2 value: "\x23" }
                          }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "unexpected priority",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554436
                        match {
                          field_id: 1
                          lpm { value: "\x10\x32\x41\x00" prefix_len: 24 }
                        }
                        priority: 32
                        action { action { action_id: 21257015 } }
                      )PB"));

  RunPiTableEntryTest(info, "action set with negative weight",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554438
                        match {
                          field_id: 1
                          lpm { value: "\xff\x00" prefix_len: 24 }
                        }
                        action {
                          action_profile_action_set {
                            action_profile_actions {
                              action {
                                action_id: 16777217
                                params { param_id: 1 value: "\000\000\000\010" }
                                params { param_id: 2 value: "\000\000\000\011" }
                              }
                              weight: -1
                            }
                          }
                        }
                      )PB"));

  RunPiTableEntryTest(info, "action set with invalid action",
                      gutil::ParseProtoOrDie<p4::v1::TableEntry>(R"PB(
                        table_id: 33554438
                        match {
                          field_id: 1
                          lpm { value: "\xff\x00" prefix_len: 24 }
                        }
                        action {
                          action_profile_action_set {
                            action_profile_actions {
                              action {
                                action_id: 16777218
                                params { param_id: 1 value: "\000\000\000\010" }
                                params { param_id: 2 value: "\000\000\000\011" }
                              }
                              weight: 1
                            }
                          }
                        }
                      )PB"));
}

static void RunIrTests(const pdpi::IrP4Info info) {
  RunIrTableEntryTest(info, "empty IR",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(""));

  RunIrTableEntryTest(info, "invalid table name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "invalid"
                      )PB"));

  RunIrTableEntryTest(info, "missing matches",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                      )PB"));

  RunIrTableEntryTest(info, "invalid match type - expect exact",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          lpm {
                            value { ipv6: "::ff22" }
                            prefix_length: 96
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "invalid match type - expect optional",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "optional_table"
                        matches {
                          name: "ipv6"
                          lpm {
                            value { ipv6: "::ff22" }
                            prefix_length: 96
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "invalid match type - expect lpm",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "lpm2_table"
                        matches {
                          name: "ipv6"
                          ternary {
                            value { ipv6: "::ff22" }
                            mask { ipv6: "::00d3:5412" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "invalid match type - expect ternary",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        priority: 32
                      )PB"));

  RunIrTableEntryTest(info, "invalid match field name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "invalid"
                          exact { ipv6: "::ff22" }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "invalid IR value",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv4"
                          exact { ipv6: "::ff22" }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "invalid prefix length",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "lpm1_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "10.32.14.2" }
                            prefix_length: 40
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "duplicate match field name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv6"
                          exact { ipv4: "10.24.32.52" }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "lpm value - masked bits set",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "lpm1_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "10.43.23.12" }
                            prefix_length: 24
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "ternary value too long",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "normal"
                          ternary {
                            value { hex_str: "0x4212" }
                            mask { hex_str: "0x00ff" }
                          }
                        }
                        priority: 32
                      )PB"));

  RunIrTableEntryTest(info, "ternary value and mask too long",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "normal"
                          ternary {
                            value { hex_str: "0x4212" }
                            mask { hex_str: "0x0fff" }
                          }
                        }
                        priority: 32
                      )PB"));

  RunIrTableEntryTest(info, "ternary value - masked bits set",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "ipv6"
                          ternary {
                            value { ipv6: "::0100" }
                            mask { ipv6: "::00ff" }
                          }
                        }
                        priority: 32
                      )PB"));

  RunIrTableEntryTest(info, "missing action",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "missing action name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                        action {}
                      )PB"));

  RunIrTableEntryTest(info, "invalid action name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                        action { name: "invalid" }
                      )PB"));

  RunIrTableEntryTest(info, "missing action params",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x54" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "duplicate action param name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x54" }
                          }
                          params {
                            name: "arg2"
                            value { hex_str: "0x65" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "invalid action param name",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg"
                            value { hex_str: "0x54" }
                          }
                          params {
                            name: "arg1"
                            value { hex_str: "0x23" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "action set in table with action",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "id_test_table"
                        matches {
                          name: "ipv6"
                          exact { ipv6: "::ff22" }
                        }
                        matches {
                          name: "ipv4"
                          exact { ipv4: "10.24.32.52" }
                        }
                        action_set {
                          actions {
                            action {
                              name: "do_thing_1"
                              params {
                                name: "arg2"
                                value { hex_str: "0x10" }
                              }
                              params {
                                name: "arg1"
                                value { hex_str: "0x11" }
                              }
                            }
                            weight: 1
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "action in table with action set",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "wcmp_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "34.234.42.0" }
                            prefix_length: 24
                          }
                        }
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x10" }
                          }
                          params {
                            name: "arg1"
                            value { hex_str: "0x11" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "zero lpm prefix length",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "lpm1_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "10.32.41.5" }
                            prefix_length: 0
                          }
                        }
                        action { name: "NoAction" }
                      )PB"));

  RunIrTableEntryTest(info, "zero ternary mask",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "normal"
                          ternary {
                            value { hex_str: "0x0100" }
                            mask { hex_str: "0x00" }
                          }
                        }
                        priority: 32
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x54" }
                          }
                          params {
                            name: "arg1"
                            value { hex_str: "0x23" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "zero priority",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "normal"
                          ternary {
                            value { hex_str: "0x0100" }
                            mask { hex_str: "0x01ff" }
                          }
                        }
                        priority: 0
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x54" }
                          }
                          params {
                            name: "arg1"
                            value { hex_str: "0x23" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "negative priority",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "normal"
                          ternary {
                            value { hex_str: "0x0100" }
                            mask { hex_str: "0x01ff" }
                          }
                        }
                        priority: -32
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x54" }
                          }
                          params {
                            name: "arg1"
                            value { hex_str: "0x23" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "absent priority",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "ternary_table"
                        matches {
                          name: "normal"
                          ternary {
                            value { hex_str: "0x0100" }
                            mask { hex_str: "0x01ff" }
                          }
                        }
                        action {
                          name: "do_thing_1"
                          params {
                            name: "arg2"
                            value { hex_str: "0x54" }
                          }
                          params {
                            name: "arg1"
                            value { hex_str: "0x23" }
                          }
                        }
                      )PB"));

  RunIrTableEntryTest(info, "unexpected priority",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "lpm1_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "10.32.41.0" }
                            prefix_length: 24
                          }
                        }
                        priority: 32
                        action { name: "NoAction" }
                      )PB"));
  RunIrTableEntryTest(info, "action set with negative weight",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "wcmp_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "0.0.255.0" }
                            prefix_length: 24
                          }
                        }
                        action_set {
                          actions {
                            action {
                              name: "do_thing_1"
                              params {
                                name: "arg2"
                                value { hex_str: "0x00000008" }
                              }
                              params {
                                name: "arg1"
                                value { hex_str: "0x00000009" }
                              }
                            }
                            weight: -1
                          }
                        }
                      )PB"));
  RunIrTableEntryTest(info, "action set with invalid action",
                      gutil::ParseProtoOrDie<pdpi::IrTableEntry>(R"PB(
                        table_name: "wcmp_table"
                        matches {
                          name: "ipv4"
                          lpm {
                            value { ipv4: "0.0.255.0" }
                            prefix_length: 24
                          }
                        }
                        action_set {
                          actions {
                            action {
                              name: "invalid_do_thing_1"
                              params {
                                name: "arg2"
                                value { hex_str: "0x00000008" }
                              }
                              params {
                                name: "arg1"
                                value { hex_str: "0x00000009" }
                              }
                            }
                            weight: -1
                          }
                        }
                      )PB"));
}

static void RunPdTests(const pdpi::IrP4Info info) {
  RunPdTableEntryTest(info, "empty PD",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(""),
                      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "missing matches",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table_entry {}
                      )PB"),
                      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "missing action", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        id_test_table_entry { match { ipv6: "::ff22" ipv4: "16.36.50.82" } }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "exact match missing",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table_entry {
                          match { ipv6: "::ff22" }
                          action {
                            do_thing_2 {
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
        lpm2_table_entry {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: -4 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "prefix length too large",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table_entry {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: 132 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "zero prefix length", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table_entry {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: 0 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "ternary entry with zero mask",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table_entry {
          match {
            normal { value: "0x52" mask: "0x00" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          priority: 32
          action { do_thing_3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "lpm value - masked bits set",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table_entry {
          match { ipv6 { value: "ffff::abcd:0:aabb" prefix_length: 96 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "ternary value - masked bits set",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table_entry {
          match {
            normal { value: "0x52" mask: "0x01" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          priority: 32
          action { do_thing_3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "action with missing arguments",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        id_test_table_entry {
          match { ipv6: "::ff22" ipv4: "16.36.50.82" }
          action {
            do_thing_2 { normal: "0x54" mac: "00:11:22:33:44:55" str: "hello" }
          }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(info, "action with wrong argument format",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table_entry {
                          match { ipv6: "::ff22" ipv4: "16.36.50.82" }
                          action {
                            do_thing_2 {
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

  RunPdTableEntryTest(
      info, "ternary table with zero priority",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table_entry {
          match {
            normal { value: "0x52" mask: "0x0273" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          priority: 0
          action { do_thing_3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "ternary table with negative priority",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table_entry {
          match {
            normal { value: "0x52" mask: "0x0273" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          priority: -43
          action { do_thing_3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "ternary table with priority absent",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table_entry {
          match {
            normal { value: "0x52" mask: "0x0273" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          action { do_thing_3 { arg1: "0x23" arg2: "0x0251" } }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "wcmp table with negative weight",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        wcmp_table_entry {
          match { ipv4 { value: "0.0.255.0" prefix_length: 24 } }
          actions {
            do_thing_1 { arg2: "0x8" arg1: "0x9" }
            weight: -1
          }
        }
      )PB"),
      INPUT_IS_INVALID);

  RunPdTableEntryTest(
      info, "valid wcmp table with choice of action",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        wcmp2_table_entry {
          match { ipv4 { value: "0.0.255.0" prefix_length: 24 } }
          actions {
            do_thing_1 { arg2: "0x8" arg1: "0x9" }
            weight: 1
          }
          actions {
            do_thing_1 { arg2: "0x10" arg1: "0x11" }
            weight: 2
          }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "valid wcmp table", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        wcmp_table_entry {
          match { ipv4 { value: "0.0.255.0" prefix_length: 24 } }
          actions {
            do_thing_1 { arg2: "0x8" arg1: "0x9" }
            weight: 1
          }
          actions {
            do_thing_1 { arg2: "0x10" arg1: "0x11" }
            weight: 2
          }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(info, "exact matches of all formats",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        exact_table_entry {
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

  RunPdTableEntryTest(info, "valid optional table missing a match",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        optional_table_entry {
                          match { ipv6 { value: "3242::fee2" } }
                          action { do_thing_1 { arg2: "0x10" arg1: "0x11" } }
                          priority: 32
                        }
                      )PB"),
                      INPUT_IS_VALID);

  RunPdTableEntryTest(info, "ternary with wildcard",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        ternary_table_entry {
                          match { normal { value: "0x52" mask: "0x273" } }
                          priority: 32
                          action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
                        }
                      )PB"),
                      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "ternary table for all formats",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        ternary_table_entry {
          match {
            normal { value: "0x52" mask: "0x273" }
            ipv4 { value: "10.43.12.4" mask: "10.43.12.5" }
            ipv6 { value: "::ee66" mask: "::ff77" }
            mac { value: "11:22:33:44:55:66" mask: "33:66:77:66:77:77" }
          }
          priority: 32
          action { do_thing_3 { arg1: "0x23" arg2: "0x251" } }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "ipv4 LPM table", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm1_table_entry {
          match { ipv4 { value: "10.43.12.0" prefix_length: 24 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(
      info, "ipv6 LPM table", gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        lpm2_table_entry {
          match { ipv6 { value: "ffff::abcd:0:0" prefix_length: 96 } }
          action { NoAction {} }
        }
      )PB"),
      INPUT_IS_VALID);

  RunPdTableEntryTest(info, "action with all formats as arguments",
                      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
                        id_test_table_entry {
                          match { ipv6: "::ff22" ipv4: "16.36.50.82" }
                          action {
                            do_thing_2 {
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

  /*
  RunPdTableEntryTest(
      info, "table entry with counters and meters",
      gutil::ParseProtoOrDie<pdpi::TableEntry>(R"PB(
        count_and_meter_table_entry {
          match { ipv4 { value: "16.36.50.0" prefix_length: 24 } }
          action { count_and_meter {} }
          meter_config { bytes_per_second: 32135 burst_bytes: 341312423 }
          byte_counter: 3123134314
          packet_counter: 390391789
        }
      )PB"),
      INPUT_IS_VALID);
      */
}

int main(int argc, char** argv) {
  CHECK(argc == 2);  // Usage: table_entry_test <p4info file>.
  const auto p4info =
      gutil::ParseProtoFileOrDie<p4::config::v1::P4Info>(argv[1]);

  absl::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(p4info);
  CHECK_OK(status_or_info.status());
  pdpi::IrP4Info info = status_or_info.value();

  RunPiTests(info);
  RunIrTests(info);
  RunPdTests(info);
  return 0;
}
