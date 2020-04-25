#include "src/meta.h"

#include <sstream>

#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"

#include "src/util.h"

namespace pdpi {
using ::p4::config::v1::MatchField;

// Copies the metadata into a stream, converts it to a string and returns it
std::string MetadataToString(const P4InfoMetadata &metadata) {
  std::stringstream ss;
  ss << "Table ID to Table Metadata" << std::endl;
  for (const auto &entry : metadata.tables) {
    ss << "Table ID: " << entry.first << std::endl
       << "\tPreamble:" << std::endl;
    p4::config::v1::Preamble preamble = entry.second.preamble;
    ss << "\t\tName: " << preamble.name() << std::endl
       << "\t\tAlias: " << preamble.alias() << std::endl;
    for (const auto &annotation : preamble.annotations()) {
      ss << "\t\tAnnotation: " << annotation << std::endl;
    }
    for (const auto &match_metadata : entry.second.match_fields) {
      MatchField match_field = match_metadata.second.match_field;
      ss << "\tMatch Field ID: " << match_metadata.first << std::endl
         << "\t\tName: " << match_field.name() << std::endl;
      for (const auto &annotation : match_field.annotations()) {
        ss << "\t\tAnnotation: " << annotation << std::endl;
      }
      ss << "\t\tBitwidth: " << match_field.bitwidth() << std::endl
         << "\t\tMatch Type: " << match_field.match_type() << std::endl;
      ss << "\t\tFormat: " << static_cast<uint32_t>(match_metadata.second.format);
    }
    ss << "\tNumber of Mandatory Match Fields: "
       << entry.second.num_mandatory_match_fields
       << std::endl;
    ss << "\tValid Actions: ";
    for (const auto &action: entry.second.valid_actions) {
      ss << action << ",";
    }
    ss << std::endl;
    ss << "\tSize: " << entry.second.size << std::endl;
  }

  ss << "Action ID to Action" << std::endl;
  for (const auto &entry : metadata.actions) {
    ss << "Action ID: " << entry.first << std::endl
       << "\tPreamble:" << std::endl;
    p4::config::v1::Preamble preamble = entry.second.preamble;
    ss << "\t\tName: " << preamble.name() << std::endl
       << "\t\tAlias: " << preamble.alias() << std::endl;
    for (const auto &param_metadata : entry.second.params) {
      p4::config::v1::Action::Param param = param_metadata.second.param;
      ss << "\tParam ID: " << param_metadata.first << std::endl
         << "\t\tName: " << param.name() << std::endl
         << "\t\tBitwidth: " << param.bitwidth() << std::endl;
      for (const auto &annotation : param.annotations()) {
        ss << "\t\tAnnotation: " << annotation << std::endl;
      }
      ss << "\t\tFormat: " << static_cast<uint32_t>(param_metadata.second.format);
    }
  }

  return ss.str();
}

// Copies the data from the P4Info into maps for easy access during translation
P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info) {
  P4InfoMetadata metadata;
  // Saves all the actions for easy access.
  for (const auto action : p4_info.actions()) {
    P4ActionMetadata action_metadata;
    const auto preamble = action.preamble();
    action_metadata.preamble = preamble;
    for (const auto param : action.params()) {
      std::vector <std::string> annotations;
      for (const auto &annotation : param.annotations()) {
        annotations.push_back(annotation);
      }
      P4ActionParamMetadata param_metadata = { param,
                                               GetFormat(annotations,
                                                         param.bitwidth()) };
      InsertIfUnique(action_metadata.params,
                     param.id(), param_metadata,
                     absl::StrCat("Duplicate param found with ID ",
                                  param.id(), " for action ",
                                  action.preamble().id(), "."));
    }
    InsertIfUnique(metadata.actions,
                   action.preamble().id(), action_metadata,
                   absl::StrCat("Duplicate action found with ID ",
                                action.preamble().id(), "."));
  }

  // Saves the table definitions into maps to have easy access to various parts
  // of it as needed.
  for (const auto &table : p4_info.tables()) {
    struct P4TableMetadata tables;
    tables.preamble = table.preamble();
    tables.num_mandatory_match_fields = 0;
    for (const auto match_field : table.match_fields()) {
      std::vector <std::string> annotations;
      for (const auto &annotation : match_field.annotations()) {
        annotations.push_back(annotation);
      }
      P4MatchFieldMetadata match_metadata = {
        match_field,
        GetFormat(annotations, match_field.bitwidth())
      };
      match_metadata.match_field = match_field;

      InsertIfUnique(tables.match_fields,
                     match_field.id(), match_metadata,
                     absl::StrCat("Duplicate match field found with ID ",
                                  match_field.id(), "."));
      if (match_field.match_type() == MatchField::EXACT) {
        ++tables.num_mandatory_match_fields;
      }
    }
    for (const auto &action_ref : table.action_refs()) {
      tables.valid_actions.insert(action_ref.id());
    }
    tables.size = table.size();
    InsertIfUnique(metadata.tables,
                   table.preamble().id(), tables,
                   absl::StrCat("Duplicate table found with ID ",
                                table.preamble().id(), "."));
  }

  return metadata;
}
} // namespace pdpi
