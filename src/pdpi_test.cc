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

#include "src/pdpi.h"

#include <memory>

#include <gtest/gtest.h>
#include "src/testdata/pdpi_proto_p4.pb.h"
#include "src/util.h"
#include "src/ir.h"

namespace pdpi {

class PdPiTest : public ::testing::Test {
 public:
  PdPiTest() {}

 protected:
  std::unique_ptr<P4InfoMetadata> p4_info_metadata_;
};

TEST_F(PdPiTest, TestPD) {
  // Place holder for testing progress during dev
  p4::config::v1::P4Info p4_info;
  ReadProtoFromFile("src/testdata/pdpi_p4info.pb.txt", &p4_info);
  P4InfoMetadata metadata = CreateMetadata(p4_info);

  p4::v1::WriteRequest write_request;
  ReadProtoFromFile("src/testdata/pi_to_pd/pdpi_pi_proto.pb.txt",
                               &write_request);

  for (const auto update : write_request.updates()) {
    p4::pdpi_proto::TableEntry pd_entry;
    PiTableEntryToPd(metadata, update.entity().table_entry(), &pd_entry);
  }
}

TEST_F(PdPiTest, TestConversion) {
  // Place holder for testing progress during dev
  p4::config::v1::P4Info p4_info;
  ReadProtoFromFile("src/testdata/pdpi_p4info.pb.txt", &p4_info);
  P4InfoMetadata metadata = CreateMetadata(p4_info);

  p4::v1::WriteRequest write_request;
  ReadProtoFromFile("src/testdata/pi_to_pd/pdpi_pi_proto.pb.txt",
                               &write_request);

  for (const auto update : write_request.updates()) {
    IrTableEntry ir = PiToIr(metadata,
                             update.entity().table_entry());
  }
}

}  // namespace pdpi
