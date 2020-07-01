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

#ifndef P4_PDPI_IR_H
#define P4_PDPI_IR_H
// P4 intermediate representation definitions for use in conversion to and from
// Program-Independent to either Program-Dependent or App-DB formats

#include "absl/container/flat_hash_map.h"
#include "gutil/status.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

class P4InfoManager {
 public:
  // Factory method that creates an instance of P4InfoManager. Returns a failure
  // if the P4Info is not well-formed.
  static gutil::StatusOr<std::unique_ptr<P4InfoManager>> Create(
      const p4::config::v1::P4Info& p4_info);

  // Returns the IR of the P4Info.
  IrP4Info GetIrP4Info() const;

  // Returns the IR of a specific table.
  gutil::StatusOr<const IrTableDefinition> GetIrTableDefinition(
      uint32_t table_id) const;

  // Returns the IR of a specific action.
  gutil::StatusOr<const IrActionDefinition> GetIrActionDefinition(
      uint32_t action_id) const;

  // Converts a PI table entry to the IR.
  gutil::StatusOr<IrTableEntry> PiTableEntryToIr(
      const p4::v1::TableEntry& pi) const;

  // Converts an IR table entry to the PI representation.
  // Not implemented yet
  // p4::v1::TableEntry IrToPi(const P4InfoMetadata &metadata,
  //                          const IrTableEntry& ir);

  // Returns the IR of a packet-io packet.
  gutil::StatusOr<IrPacketIn> PiPacketInToIr(
      const p4::v1::PacketIn& packet) const;
  gutil::StatusOr<IrPacketOut> PiPacketOutToIr(
      const p4::v1::PacketOut& packet) const;

  // Returns the PI of a packet-io packet.
  gutil::StatusOr<p4::v1::PacketIn> IrPacketInToPi(
      const IrPacketIn& packet) const;
  gutil::StatusOr<p4::v1::PacketOut> IrPacketOutToPi(
      const IrPacketOut& packet) const;

 protected:
  P4InfoManager() {}

 private:
  // Translates the action invocation from its PI form to IR.
  gutil::StatusOr<IrActionInvocation> PiActionInvocationToIr(
      const p4::v1::TableAction& pi_table_action,
      const google::protobuf::RepeatedPtrField<IrActionDefinition>&
          valid_actions) const;

  // Generic helper that works for both packet-in and packet-out. For both, I is
  // one of p4::v1::{PacketIn, PacketOut} and O is one of {IrPacketIn,
  // IrPacketOut}.
  template <typename I, typename O>
  gutil::StatusOr<O> PiPacketIoToIr(const std::string& kind,
                                    const I& packet) const;
  template <typename I, typename O>
  gutil::StatusOr<I> IrPacketIoToPi(const std::string& kind,
                                    const O& packet) const;

  // The parsed P4Info.
  IrP4Info info_;

  // Maps table IDs to the number of mandatory match fields in that table.
  absl::flat_hash_map<uint32_t, int> num_mandatory_match_fields_;
};

}  // namespace pdpi
#endif  // P4_PDPI_IR_H
