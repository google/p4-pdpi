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
#include "grpcpp/grpcpp.h"
#include "gutil/status.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

// Creates IrP4Info and validates that the p4_info has no errors.
gutil::StatusOr<IrP4Info> CreateIrP4Info(const p4::config::v1::P4Info& p4_info);

// Converts a PI table entry to the IR table entry.
gutil::StatusOr<IrTableEntry> PiTableEntryToIr(const IrP4Info& info,
                                               const p4::v1::TableEntry& pi);

// Converts an IR table entry to the PI representation.
gutil::StatusOr<p4::v1::TableEntry> IrTableEntryToPi(const IrP4Info& info,
                                                     const IrTableEntry& ir);

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

// RPC-level conversion functions for read request
gutil::StatusOr<IrReadRequest> PiReadRequestToIr(
    const IrP4Info& info, const p4::v1::ReadRequest& read_request);
gutil::StatusOr<p4::v1::ReadRequest> IrReadRequestToPi(
    const IrP4Info& info, const IrReadRequest& read_request);

// RPC-level conversion functions for read response
gutil::StatusOr<IrReadResponse> PiReadResponseToIr(
    const IrP4Info& info, const p4::v1::ReadResponse& read_response);
gutil::StatusOr<p4::v1::ReadResponse> IrReadResponseToPi(
    const IrP4Info& info, const IrReadResponse& read_response);

// RPC-level conversion functions for update
gutil::StatusOr<IrUpdate> PiUpdateToIr(const IrP4Info& info,
                                       const p4::v1::Update& update);
gutil::StatusOr<p4::v1::Update> IrUpdateToPi(const IrP4Info& info,
                                             const IrUpdate& update);

// RPC-level conversion functions for write request
gutil::StatusOr<IrWriteRequest> PiWriteRequestToIr(
    const IrP4Info& info, const p4::v1::WriteRequest& write_request);
gutil::StatusOr<p4::v1::WriteRequest> IrWriteRequestToPi(
    const IrP4Info& info, const IrWriteRequest& write_request);

// RPC-level conversion functions for write response
gutil::StatusOr<IrWriteRpcStatus> GrpcStatusToIrWriteRpcStatus(
    const grpc::Status& status, int number_of_updates_in_write_request);
grpc::Status IrWriteResponseToGrpcStatus(const IrWriteResponse response);

}  // namespace pdpi
#endif  // P4_PDPI_IR_H
