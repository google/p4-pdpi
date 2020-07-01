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

// Creates IrP4Info and validates that the p4_info has no errors.
gutil::StatusOr<IrP4Info> CreateIrP4Info(const p4::config::v1::P4Info& p4_info);

// Converts a PI table entry to the IR.
gutil::StatusOr<IrTableEntry> PiTableEntryToIr(const IrP4Info& info,
                                               const p4::v1::TableEntry& pi);

// Converts an IR table entry to the PI representation.
// Not implemented yet
// p4::v1::TableEntry IrTableEntryToPi(const IrP4Info& info,,
//                          const IrTableEntry& ir);

// Returns the IR of a packet-io packet.
gutil::StatusOr<IrPacketIn> PiPacketInToIr(const IrP4Info& info,
                                           const p4::v1::PacketIn& packet);
gutil::StatusOr<IrPacketOut> PiPacketOutToIr(const IrP4Info& info,
                                             const p4::v1::PacketOut& packet);

// Returns the PI of a packet-io packet.
gutil::StatusOr<p4::v1::PacketIn> IrPacketInToPi(const IrP4Info& info,
                                                 const IrPacketIn& packet);
gutil::StatusOr<p4::v1::PacketOut> IrPacketOutToPi(const IrP4Info& info,
                                                   const IrPacketOut& packet);

}  // namespace pdpi
#endif  // P4_PDPI_IR_H
