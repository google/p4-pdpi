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

TEST_F(P4InfoMetadataTest, TestCreate) {
  // This test currently does nothing. Just calls create metadata to prove that
  // the infra works.
  p4::config::v1::P4Info p4_info;
  P4InfoMetadata p4_metadata;
  ReadProtoFromFile("testdata/pdpi_p4info.pb.txt", &p4_info);
  p4_metadata = CreateMetadata(p4_info);
}

}  // namespace
