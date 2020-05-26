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

#include "p4/v1/p4runtime.pb.h"
#include "src/ir.pb.h"
#include "src/meta.h"

namespace pdpi {

class P4InfoManager {
 public:
  // Parses the P4Info proto. Throws std::invalid_argument if the P4Info is not
  // well-formed.
  P4InfoManager(const p4::config::v1::P4Info &p4_info);

  // Converts a PI table entry to the IR. Throws std::invalid_argument if PI is
  // not well-formed, and throws a pdpi::internal_error exception (which is a
  // std::runtime_error) on internal errors.
  pdpi::ir::IrTableEntry PiTableEntryToIr(const p4::v1::TableEntry &pi);

  // Converts an IR table entry to the PI representation. Throws
  // std::invalid_argument if PI is not well-formed, and throws a
  // pdpi::internal_error exception (which is a std::runtime_error) on internal
  // errors.
  // Not implemented yet
  // p4::v1::TableEntry IrToPi(const P4InfoMetadata &metadata,
  //                          const IrTableEntry& ir);
 private:
  // Translates the action from its PI form to IR
  pdpi::ir::IrAction PiActionToIr(
      const p4::v1::TableAction &pi_table_action,
      const absl::flat_hash_set<uint32_t> &valid_actions);

  P4InfoMetadata p4_metadata_;
};
}  // namespace pdpi
#endif  // PDPI_IR_H
