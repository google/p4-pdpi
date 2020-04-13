#include "pdpi.h"
#include <gtest/gtest.h>
#include <memory>
#include "util.h"
#include "testdata/pdpi_proto_p4.pb.h"

namespace pdpi {

class PdPiTest : public ::testing::Test {
 public:
  PdPiTest() {}

 protected:
  std::unique_ptr<P4InfoMetadata> p4_info_metadata_;
};

TEST_F(PdPiTest, TestCreateMetadataDuplicateActionID) {
  p4::config::v1::P4Info p4_info;
  ReadProtoFromString(R"PROTO(
  actions {
    preamble {
      id: 16777217
    }
  }
  actions {
    preamble {
      id: 16777217
    }
  }
  )PROTO", &p4_info);

  EXPECT_THROW(CreateMetadata(p4_info), std::invalid_argument);
}

TEST_F(PdPiTest, TestCreateMetadataDuplicateMatchFieldID) {
  p4::config::v1::P4Info p4_info;
  ReadProtoFromString(R"PROTO(
  tables {
    match_fields {
      id: 1
    }
    match_fields {
      id: 1
    }
  }
  )PROTO", &p4_info);

  EXPECT_THROW(CreateMetadata(p4_info), std::invalid_argument);
}

TEST_F(PdPiTest, TestCreateMetadataDuplicateTableID) {
  p4::config::v1::P4Info p4_info;
  ReadProtoFromString(R"PROTO(
  tables {
    preamble {
      id: 33554433
    }
  }
  tables {
    preamble {
      id: 33554433
    }
  }
  )PROTO", &p4_info);

  EXPECT_THROW(CreateMetadata(p4_info), std::invalid_argument);
}

TEST_F(PdPiTest, TestPD) {
  // Place holder for testing progress during dev
  p4::config::v1::P4Info p4_info;
  ReadProtoFromFile("testdata/pdpi_p4info.pb.txt", &p4_info);
  P4InfoMetadata metadata = CreateMetadata(p4_info);

  p4::v1::TableEntry pi_entry;
  ReadProtoFromFile("testdata/pi_to_pd/pdpi_pi_proto.pb.txt", &pi_entry);

  p4::pdpi_proto::TableEntry pd_entry;
  PiToPd(metadata, pi_entry, &pd_entry);
}

}  // namespace pdpi
