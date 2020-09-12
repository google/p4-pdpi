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

#ifndef GOOGLE_P4_PDPI_ENTITY_MANAGEMENT_H_
#define GOOGLE_P4_PDPI_ENTITY_MANAGEMENT_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/connection_management.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

// Sends a PI (program independent) read request.
absl::StatusOr<p4::v1::ReadResponse> SendPiReadRequest(
    P4RuntimeSession* session, const p4::v1::ReadRequest& read_request);

// Sends a PI (program independent) write request.
absl::Status SendPiWriteRequest(P4RuntimeSession* session,
                                const p4::v1::WriteRequest& write_request);

// Reads PI (program independent) table entries.
absl::StatusOr<p4::v1::ReadResponse> ReadPiTableEntries(
    P4RuntimeSession* session);

// Removes PI (program independent) entities.
absl::Status RemovePiEntities(P4RuntimeSession* session,
                              absl::Span<const p4::v1::Entity* const> entities);

// Clears the table entries
absl::Status ClearTableEntries(P4RuntimeSession* session, const IrP4Info& info);

// Installs the given PI (program independent) table entry on the switch.
absl::Status InstallPiTableEntry(P4RuntimeSession* session,
                                 const p4::v1::TableEntry& pi_entry);

}  // namespace pdpi

#endif  // GOOGLE_P4_PDPI_ENTITY_MANAGEMENT_H_
