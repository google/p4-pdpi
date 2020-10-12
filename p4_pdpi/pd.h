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

#ifndef P4_PDPI_PD_H
#define P4_PDPI_PD_H

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/message.h"
#include "gutil/status.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

// This file contains functions that translate from and to PD.
// Since the exact form of PD is not known until run time, we need to pass in
// a generic google::protobuf::Message and use GetReflection() to access the PD
// proto.

// -- Conversions to and from PI -----------------------------------------------

absl::Status PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                              const p4::v1::TableEntry &pi,
                              google::protobuf::Message *pd);

absl::StatusOr<p4::v1::TableEntry> PdTableEntryToPi(
    const p4::config::v1::P4Info &p4_info, const google::protobuf::Message &pd);

absl::Status PiPacketInToPd(const IrP4Info &info,
                            const p4::v1::PacketIn &pi_packet,
                            google::protobuf::Message *pd_packet);

absl::StatusOr<p4::v1::PacketIn> PdPacketInToPi(
    const IrP4Info &info, const google::protobuf::Message &packet);

absl::Status PiPacketOutToPd(const IrP4Info &info,
                             const p4::v1::PacketOut &pi_packet,
                             google::protobuf::Message *pd_packet);

absl::StatusOr<p4::v1::PacketOut> PdPacketOutToPi(
    const IrP4Info &info, const google::protobuf::Message &packet);

// -- Conversions to and from grpc::Status -------------------------------------

absl::Status GrpcStatusToPd(const grpc::Status &status,
                            int number_of_updates_in_write_request,
                            google::protobuf::Message *pd);

absl::StatusOr<grpc::Status> PdWriteRpcStatusToGrpcStatus(
    const google::protobuf::Message &pd);

// -- Conversions to and from the IR (intermediate representation) -------------

absl::Status IrReadRequestToPd(const IrP4Info &info, const IrReadRequest &ir,
                               google::protobuf::Message *pd);
absl::StatusOr<IrReadRequest> PdReadRequestToIr(
    const IrP4Info &info, const google::protobuf::Message &read_request);

absl::Status IrReadResponseToPd(const IrP4Info &info, const IrReadResponse &ir,
                                google::protobuf::Message *read_response);
absl::StatusOr<IrReadResponse> PdReadResponseToIr(
    const IrP4Info &info, const google::protobuf::Message &read_response);

absl::Status IrUpdateToPd(const IrP4Info &info, const IrUpdate &ir,
                          google::protobuf::Message *update);
absl::StatusOr<IrUpdate> PdUpdateToIr(const IrP4Info &info,
                                      const google::protobuf::Message &update);

absl::Status IrWriteRequestToPd(const IrP4Info &info, const IrWriteRequest &ir,
                                google::protobuf::Message *write_request);
absl::StatusOr<IrWriteRequest> PdWriteRequestToIr(
    const IrP4Info &info, const google::protobuf::Message &write_request);

// Converts a PD table entry to the IR table entry.
absl::StatusOr<IrTableEntry> PdTableEntryToIr(
    const IrP4Info &ir_p4info, const google::protobuf::Message &pd);

// Converts an IR table entry to the PD table entry.
absl::Status IrTableEntryToPd(const IrP4Info &ir_p4info, const IrTableEntry &ir,
                              google::protobuf::Message *pd);

// Converts an IR write response to PD write response.
absl::Status IrRpcResponseToPd(const IrWriteResponse &ir_rpc_response,
                               google::protobuf::Message *pd_rpc_response);
// Converts an IR write status to PD write status.
absl::Status IrWriteRpcStatusToPd(const IrWriteRpcStatus &ir_write_status,
                                  google::protobuf::Message *pd);

// Converts a PD write status to IR write status.
absl::StatusOr<IrWriteRpcStatus> PdWriteRpcStatusToIr(
    const google::protobuf::Message &pd);

absl::StatusOr<IrPacketIn> PdPacketInToIr(
    const IrP4Info &info, const google::protobuf::Message &packet);
absl::StatusOr<IrPacketOut> PdPacketOutToIr(
    const IrP4Info &info, const google::protobuf::Message &packet);
absl::Status IrPacketInToPd(const IrP4Info &info, const IrPacketIn &packet,
                            google::protobuf::Message *pd_packet);
absl::Status IrPacketOutToPd(const IrP4Info &info, const IrPacketOut &packet,
                             google::protobuf::Message *pd_packet);

// -- PD getters/setters -------------------------------------------------------

// Get Enum field in proto message's field with `field_name`.
absl::StatusOr<int> GetEnumField(const google::protobuf::Message &message,
                                 const std::string &field_name);

// Set Enum field in proto message's field with `field_name` to value with
// tag number == `enum_value`.
absl::Status SetEnumField(google::protobuf::Message *message,
                          const std::string &enum_field_name, int enum_value);

}  // namespace pdpi

#endif  // P4_PDPI_PD_H
