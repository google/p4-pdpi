#include "src/pdpi.h"

#include "src/ir.h"
#include "src/ir.pb.h"

namespace pdpi {
using google::protobuf::FieldDescriptor;
using ::p4::config::v1::MatchField;

// Translate all matches from their IR form to the PD representations
void IrToPd(const pdpi::ir::IrTableEntry &ir, google::protobuf::Message *pd) {
  // Commented out till new PD definition is available
  /*
  auto *pd_table_entry =
      GetMessageByFieldname(TableEntryFieldname(ir.table_name), pd);

  // Copy over the FieldMatches
  auto *pd_match_entry =
      GetMessageByFieldname(kFieldMatchFieldname, pd_table_entry);
  for (const auto ir_match : ir.matches) {
    std::string fieldname = ProtoFriendlyName(ir_match.name);
    auto *field = GetFieldDescriptorByName(fieldname, pd_match_entry);
    absl::visit(overloaded{
                    [&pd_match_entry, &field](const std::string &s) {
                      pd_match_entry->GetReflection()->SetString(pd_match_entry,
                                                                 field, s);
                    },
                    [&pd_match_entry, &field](const IrTernaryMatch &ternary) {
                      auto *value_field = GetFieldDescriptorByName(
                          kTernaryValueFieldname, pd_match_entry);
                      pd_table_entry->GetReflection()->SetString(
                          pd_match_entry, value_field, ternary.value);

                      auto *mask_field = GetFieldDescriptorByName(
                          kTernaryMaskFieldname, pd_match_entry);
                      pd_table_entry->GetReflection()->SetString(
                          pd_match_entry, mask_field, ternary.mask);
                    },
                },
                ir_match.value);
  }

  // Copy over the Action if any
  if (ir.action.has_value()) {
    auto *pd_action_entry =
        GetMessageByFieldname(kActionFieldname, pd_table_entry);
    auto *pd_oneof_action = GetMessageByFieldname(
        ActionFieldname(ir.action.value().name), pd_action_entry);
    for (const auto &param : ir.action.value().params) {
      auto *field = GetFieldDescriptorByName(ProtoFriendlyName(param.name),
                                             pd_oneof_action);
      pd_oneof_action->GetReflection()->SetString(pd_oneof_action, field,
                                                  param.value);
    }
  }
  */
}

void PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                      const p4::v1::TableEntry &pi,
                      google::protobuf::Message *pd) {
  P4InfoManager ir(p4_info);
  pdpi::ir::IrTableEntry ir_entry = ir.PiTableEntryToIr(pi);
  IrToPd(ir_entry, pd);
}
}  // namespace pdpi
