#ifndef PDPI_H
#define PDPI_H

#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"

namespace pdpi {
struct P4TableMetadata {
  p4::config::v1::Preamble preamble;
  // Maps match field IDs to match fields.
  std::unordered_map<uint32_t, p4::config::v1::MatchField> match_fields;
  std::unordered_set<uint32_t> valid_actions;
  uint32_t size;
};

struct P4ActionMetadata {
  p4::config::v1::Preamble preamble;
  std::unordered_map<uint32_t, p4::config::v1::Action::Param> params;
};

struct P4InfoMetadata {
  // Maps table IDs to table metadata.
  std::unordered_map<uint32_t, struct P4TableMetadata> tables;
  // Maps action IDs to actions.
  std::unordered_map<uint32_t, P4ActionMetadata> actions;
};

// Creates the metadata needed for conversion from PI to PD and viceversa.
P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info);
// Returns the metadata as a string.
std::string MetadataToString(const P4InfoMetadata &metadata);
// Translates given pi proto to pd proto using provided metadata
void PiToPd(const P4InfoMetadata &metadata,
            const p4::v1::TableEntry &pi,
            google::protobuf::Message *pd);
}  // namespace pdpi

#endif  // PDPI_H
