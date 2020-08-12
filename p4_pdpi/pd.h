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

#include "absl/status/status.h"
#include "gutil/status.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {
// This file contains functions that translate from and to PD.
// Since the exact form of PD is not known till run time, we
// need pass in a generic google::protobuf::Message and use
// GetReflection() to get the PD proto.

constexpr char kPdProtoAndP4InfoOutOfSync[] =
    "The PD proto and P4Info file are out of sync.";

absl::Status PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                              const p4::v1::TableEntry &pi,
                              google::protobuf::Message *pd);

gutil::StatusOr<p4::v1::TableEntry> PdTableEntryToPi(
    const p4::config::v1::P4Info &p4_info, const google::protobuf::Message &pd);

absl::Status IrReadRequestToPd(const IrP4Info &info, const IrReadRequest &ir,
                               google::protobuf::Message *pd);
gutil::StatusOr<IrReadRequest> PdReadRequestToIr(
    const IrP4Info &info, const google::protobuf::Message &read_request);

// Converts a PD table entry to the IR table entry.
gutil::StatusOr<IrTableEntry> PdTableEntryToIr(
    const IrP4Info &ir_p4info, const google::protobuf::Message &pd);

// Converts an IR table entry to the PD table entry.
absl::Status IrTableEntryToPd(const IrP4Info &ir_p4info, const IrTableEntry &ir,
                              google::protobuf::Message *pd);

// Converts an IR write status to PD write status.
absl::Status IrWriteRpcStatusToPd(const IrWriteRpcStatus &status,
                                  google::protobuf::Message *pd);

// Converts a PD write status to IR write status.
gutil::StatusOr<IrWriteRpcStatus> PdWriteRpcStatusToIr(
    const google::protobuf::Message &pd);
}  // namespace pdpi

#endif  // P4_PDPI_PD_H
