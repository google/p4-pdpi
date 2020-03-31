#include "pdpi.h"
#include <gtest/gtest.h>
#include <memory>
#include "util.h"

namespace {

class P4InfoMetadataTest : public ::testing::Test {
 public:
  P4InfoMetadataTest() {}

 protected:
  std::unique_ptr<P4InfoMetadata> p4_info_metadata_;
};

TEST_F(P4InfoMetadataTest, TestCreateMetadataDuplicateActionID) {
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

TEST_F(P4InfoMetadataTest, TestCreateMetadataDuplicateMatchFieldID) {
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

TEST_F(P4InfoMetadataTest, TestCreateMetadataDuplicateTableID) {
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

}  // namespace
