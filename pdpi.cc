#include "pdpi.h"
#include <sstream>
#include "absl/strings/str_cat.h"
#include "util.h"

std::string MetadataToString(const P4InfoMetadata &metadata) {
  std::stringstream ss;
  ss << "Table ID to Table Metadata" << std::endl;
  for (const auto &entry : metadata.tables) {
    ss << "Table ID: " << entry.first << std::endl
       << "\tPreamble: " << std::endl;
    p4::config::v1::Preamble preamble = entry.second.preamble;
    ss << "\t\tName: " << preamble.name() << std::endl
       << "\t\tAlias: " << preamble.alias() << std::endl;
    for (const auto &annotation : preamble.annotations()) {
      ss << "\t\tAnnotation: " << annotation << std::endl;
    }
    for (const auto &match_field : entry.second.match_fields) {
      ss << "\tMatch Field ID: " << match_field.first << std::endl
         << "\t\tName: " << match_field.second.name() << std::endl;
      for (const auto &annotation : match_field.second.annotations()) {
        ss << "\t\tAnnotation: " << annotation << std::endl;
      }
      ss << "\t\tBitwidth: " << match_field.second.bitwidth() << std::endl
         << "\t\tMatch Type: " << match_field.second.match_type() << std::endl;
    }
    ss << "\tValid Actions: ";
    for (const auto &action: entry.second.valid_actions) {
      ss << action << ", ";
    }
    ss << std::endl;
    ss << "\tSize: " << entry.second.size << std::endl;
  }

  ss << "Action ID to Action" << std::endl;
  for (const auto &entry : metadata.actions) {
    ss << "Action ID: " << entry.first << std::endl
       << "\tPreamble: " << std::endl;
    p4::config::v1::Preamble preamble = entry.second.preamble();
    ss << "\t\tName: " << preamble.name() << std::endl
       << "\t\tAlias: " << preamble.alias() << std::endl;
    for (const auto &param : entry.second.params()) {
      ss << "\tParam ID: " << param.id() << std::endl
         << "\t\tName: " << param.name() << std::endl
         << "\t\tBitwidth: " << param.bitwidth() << std::endl;
      for (const auto &annotation : param.annotations()) {
        ss << "\t\tAnnotation: " << annotation << std::endl;
      }
    }
  }

  return ss.str();
}

P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info) {
  P4InfoMetadata metadata;
  // Saves all the actions for easy access.
  for (const auto action : p4_info.actions()) {
    const auto &action_insert = metadata.actions.insert(
        {action.preamble().id(), action});
    if (!action_insert.second) {
      throw std::invalid_argument(absl::StrCat("Duplicate action found with ",
                                               "ID ",action.preamble().name()));
    }
  }

  // Saves the table definitions into maps to have easy access to various parts
  // of it as needed.
  for (const auto &table : p4_info.tables()) {
    // Not using reference since preamble will be stored in map and P4Info may
    // or may not be available at that point.
    // Will make a copy on heap in future since same memory can be pointed to in
    // maps used for PD to PI conversions.
    // Same case for match_field below.
    struct P4TableMetadata tables;
    const auto preamble = table.preamble();
    tables.preamble = preamble;
    for (const auto match_field : table.match_fields()) {
      const auto &match_insert = tables.match_fields.insert(
          {match_field.id(), match_field});
      if (!match_insert.second) {
        throw std::invalid_argument(absl::StrCat("Duplicate match_field found ",
                                                 "with ID ", match_field.id()));
      }
    }
    for (const auto &action_ref : table.action_refs()) {
      tables.valid_actions.push_back(action_ref.id());
    }
    tables.size = table.size();
    const auto &table_insert = metadata.tables.insert(
        {table.preamble().id(), tables});
    if (!table_insert.second) {
      throw std::invalid_argument(absl::StrCat("Duplicate table found with ",
                                               "ID ", table.preamble().id()));
    }
  }

  return metadata;
}
