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
#include "p4_pdpi/testing/test_helper.h"
#include "tools/cpp/runfiles/runfiles.h"

using ::bazel::tools::cpp::runfiles::Runfiles;
using ::p4::config::v1::P4Info;

void RunP4InfoTest(const std::string& test_name, const P4Info& p4info) {
  std::cout << TestHeader(test_name) << std::endl << std::endl;
  std::cout << "P4Info input:" << std::endl;
  std::cout << p4info.DebugString() << std::endl;
  gutil::StatusOr<pdpi::IrP4Info> status_or_info = pdpi::CreateIrP4Info(p4info);
  std::cout << "pdpi::CreateIrP4Info() result:" << std::endl;
  if (!status_or_info.ok()) {
    std::cout << status_or_info.status() << std::endl;
  } else {
    std::cout << status_or_info.value().DebugString() << std::endl;
  }
  std::cout << std::endl;
}

int main(int argc, char** argv) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
  CHECK(runfiles != nullptr);

  RunP4InfoTest("missing action definition",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(tables {
                           preamble { id: 1 name: "table1" alias: "table1" }
                           action_refs {
                             id: 1  # not defined
                           }
                           size: 1024
                         })PB"));

  RunP4InfoTest("duplicate table id",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(tables {
                           preamble { id: 1 name: "table1" alias: "table1" }
                         }
                         tables {
                           preamble { id: 1 name: "table2" alias: "table2" }
                         })PB"));

  RunP4InfoTest("duplicate match field id",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(tables {
                           preamble { id: 1 name: "table1" alias: "table1" }
                           match_fields {
                             id: 1
                             name: "field1"
                             bitwidth: 1
                             match_type: EXACT
                           }
                           match_fields {
                             id: 1
                             name: "field2"
                             bitwidth: 1
                             match_type: EXACT
                           }
                         })PB"));

  RunP4InfoTest("duplicate action id",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(actions {
                           preamble { id: 1 name: "action1" alias: "action1" }
                         }
                         actions {
                           preamble { id: 1 name: "action2" alias: "action2" }
                         })PB"));

  RunP4InfoTest("duplicate param id",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(actions {
                           preamble { id: 1 name: "action1" alias: "action1" }
                           params { id: 1 name: "param1" }
                           params { id: 1 name: "param2" }
                         })PB"));

  RunP4InfoTest("duplicate table name",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(tables {
                           preamble { id: 1 name: "table1" alias: "table1" }
                         }
                         tables {
                           preamble { id: 2 name: "table2" alias: "table1" }
                         })PB"));

  RunP4InfoTest("duplicate match field name",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(tables {
                           preamble { id: 1 name: "table1" alias: "table1" }
                           match_fields {
                             id: 1
                             name: "field1"
                             bitwidth: 1
                             match_type: EXACT
                           }
                           match_fields {
                             id: 2
                             name: "field1"
                             bitwidth: 1
                             match_type: EXACT
                           }
                         })PB"));

  RunP4InfoTest("duplicate action name",
                gutil::ParseProtoOrDie<P4Info>(
                    R"PB(actions {
                           preamble { id: 1 name: "action1" alias: "action1" }
                         }
                         actions {
                           preamble { id: 2 name: "action2" alias: "action1" }
                         })PB"));

  RunP4InfoTest("duplicate param name", gutil::ParseProtoOrDie<P4Info>(R"PB(
                  actions {
                    preamble { id: 1 name: "action1" alias: "action1" }
                    params { id: 1 name: "param1" }
                    params { id: 2 name: "param1" }
                  })PB"));

  RunP4InfoTest("main.p4", GetP4Info(runfiles.get(),
                                     "p4_pdpi/testing/main-p4info.pb.txt"));

  return 0;
}
