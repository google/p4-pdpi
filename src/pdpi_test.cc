#include "src/pdpi.h"

#include <gtest/gtest.h>

#include <memory>

#include "src/ir.h"
#include "src/ir.pb.h"
#include "src/testdata/pdpi_proto_p4.pb.h"
#include "src/util.h"

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

  p4::v1::WriteRequest write_request;
  ReadProtoFromFile("src/testdata/pi_to_pd/pdpi_pi_proto.pb.txt",
                    &write_request);

  for (const auto update : write_request.updates()) {
    p4::pdpi_proto::TableEntry pd_entry;
    PiTableEntryToPd(p4_info, update.entity().table_entry(), &pd_entry);
  }
}

TEST_F(PdPiTest, TestConversion) {
  // Place holder for testing progress during dev
  p4::config::v1::P4Info p4_info;
  ReadProtoFromFile("src/testdata/pdpi_p4info.pb.txt", &p4_info);

  p4::v1::WriteRequest write_request;
  ReadProtoFromFile("src/testdata/pi_to_pd/pdpi_pi_proto.pb.txt",
                    &write_request);

  P4InfoManager ir(p4_info);
  for (const auto update : write_request.updates()) {
    pdpi::ir::IrTableEntry ir_table =
        ir.PiTableEntryToIr(update.entity().table_entry());
  }
}

}  // namespace pdpi
