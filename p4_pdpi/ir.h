// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PDPI_IR_H
#define PDPI_IR_H
// P4 intermediate representation definitions for use in conversion to and from
// Program-Independent to either Program-Dependent or App-DB formats

#include "absl/container/flat_hash_map.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/status_utils.h"

namespace pdpi {

class P4InfoManager {
 public:
  // Factory method that creates an instance of P4InfoManager. Returns a failure
  // if the P4Info is not well-formed.
  static StatusOr<std::unique_ptr<P4InfoManager>> Create(
      const p4::config::v1::P4Info &p4_info);

  // Returns the IR of the P4Info.
  ir::IrP4Info GetIrP4Info() const;

  // Returns the IR of a specific table.
  StatusOr<ir::IrTableDefinition> GetIrTableDefinition(uint32_t table_id) const;

  // Returns the IR of a specific action.
  StatusOr<ir::IrActionDefinition> GetIrActionDefinition(
      uint32_t action_id) const;

  // Converts a PI table entry to the IR.
  StatusOr<ir::IrTableEntry> PiTableEntryToIr(
      const p4::v1::TableEntry &pi) const;

  // Converts an IR table entry to the PI representation.
  // Not implemented yet
  // p4::v1::TableEntry IrToPi(const P4InfoMetadata &metadata,
  //                          const IrTableEntry& ir);
 protected:
  P4InfoManager() {}

 private:
  // Translates the action invocation from its PI form to IR.
  StatusOr<ir::IrActionInvocation> PiActionInvocationToIr(
      const p4::v1::TableAction &pi_table_action,
      const google::protobuf::RepeatedPtrField<ir::IrActionDefinition>
          &valid_actions) const;

  // The parsed P4Info.
  ir::IrP4Info info_;

  // Maps table IDs to the number of mandatory match fields in that table.
  absl::flat_hash_map<uint32_t, int> num_mandatory_match_fields_;
};
}  // namespace pdpi
#endif  // PDPI_IR_H
