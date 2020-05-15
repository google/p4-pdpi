#include "src/meta.h"

#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "p4/config/v1/p4info.pb.h"
#include "src/util.h"

namespace pdpi {
using ::p4::config::v1::MatchField;

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

P4InfoMetadata CreateMetadata(const p4::config::v1::P4Info &p4_info) {
  P4InfoMetadata metadata;
  // Saves all the actions for easy access.
  absl::flat_hash_set<std::string> action_names;
  for (const auto action : p4_info.actions()) {
    P4ActionMetadata action_metadata;
    InsertIfUnique(action_names, action.preamble().alias(),
                   absl::StrCat("Duplicate action name ",
                                action.preamble().alias(), " found."));
    action_metadata.preamble = action.preamble();
    absl::flat_hash_set<std::string> param_names;
    for (const auto param : action.params()) {
      std::vector <std::string> annotations;
      for (const auto &annotation : param.annotations()) {
        annotations.push_back(annotation);
      }
      P4ActionParamMetadata param_metadata;
      InsertIfUnique(
          param_names, param.name(),
          absl::StrCat("Duplicate param name ", param.name(),
                       " found in action ", action.preamble().alias(), "."));
      absl::optional<std::string> named_type;
      if (param.has_type_name()) {
        named_type = param.type_name().name();
      }
      param_metadata = { param, GetFormat(annotations,
                                          param.bitwidth(),
                                          named_type) };
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

  absl::flat_hash_set<std::string> table_names;
  // Saves the table definitions into maps to have easy access to various parts
  // of it as needed.
  for (const auto &table : p4_info.tables()) {
    struct P4TableMetadata tables;
    InsertIfUnique(table_names, table.preamble().alias(),
                   absl::StrCat("Duplicate table name ",
                                table.preamble().alias(), " found."));
    tables.preamble = table.preamble();
    tables.num_mandatory_match_fields = 0;
    absl::flat_hash_set<std::string> match_field_names;
    for (const auto match_field : table.match_fields()) {
      std::vector <std::string> annotations;
      for (const auto &annotation : match_field.annotations()) {
        annotations.push_back(annotation);
      }
      P4MatchFieldMetadata match_metadata;

      InsertIfUnique(
          match_field_names, match_field.name(),
          absl::StrCat("Duplicate match field name ", match_field.name(),
                       " found in table ", table.preamble().alias(), "."));
      absl::optional<std::string> named_type;
      if (match_field.has_type_name()) {
        named_type = match_field.type_name().name();
      }
      match_metadata = { match_field, GetFormat(annotations,
                                                match_field.bitwidth(),
                                                named_type) };
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
      // Make sure the action is defined
      FindElement(metadata.actions, action_ref.id(),
                  absl::StrCat("Missing definition for action with id ",
                               action_ref.id(), "."));
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
