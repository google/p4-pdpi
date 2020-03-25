#ifndef PDPI_H
#define PDPI_H

#include "p4/config/v1/p4info.pb.h"

struct P4TableMetadata {
  p4::config::v1::Preamble preamble;
  // Maps match field IDs to match fields.
  std::unordered_map<uint32_t, p4::config::v1::MatchField> match_fields;
  std::vector<uint32_t> valid_actions;
  uint32_t size;
};
struct P4InfoMetadata {
  // Maps table IDs to table metadata.
  std::unordered_map<uint32_t, struct P4TableMetadata> tables;
  // Maps action IDs to actions.
  std::unordered_map<uint32_t, p4::config::v1::Action> actions;
};

// Creates the metadata needed for conversion from PI to PD and viceversa.
P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info);
// Returns the metadata as a string.
std::string MetadataToString(const P4InfoMetadata &metadata);

#endif  // PDPI_H
