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

#include "p4_pdpi/entity_management.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_field.h"
#include "gutil/status.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/connection_management.h"
#include "p4_pdpi/ir.h"
#include "p4_pdpi/ir.pb.h"
#include "p4_pdpi/utils/ir.h"

namespace pdpi {

using ::p4::v1::Entity;
using ::p4::v1::ReadRequest;
using ::p4::v1::ReadResponse;
using ::p4::v1::Update;
using ::p4::v1::WriteRequest;
using ::p4::v1::WriteResponse;

absl::StatusOr<ReadResponse> SendPiReadRequest(
    P4RuntimeSession* session, const ReadRequest& read_request) {
  grpc::ClientContext context;
  auto reader = session->Stub().Read(&context, read_request);

  ReadResponse response;
  ReadResponse partial_response;
  while (reader->Read(&partial_response)) {
    response.MergeFrom(partial_response);
  }
  RETURN_IF_ERROR(reader->Finish());
  return response;
}

absl::Status SendPiWriteRequest(P4RuntimeSession* session,
                                const WriteRequest& write_request) {
  grpc::ClientContext context;
  // Empty message; intentionally discarded.
  WriteResponse pi_response;
  return GrpcStatusToAbslStatus(
      session->Stub().Write(&context, write_request, &pi_response),
      write_request.updates_size());
}

absl::StatusOr<ReadResponse> ReadPiTableEntries(P4RuntimeSession* session) {
  ReadRequest read_request;
  read_request.add_entities()->mutable_table_entry();
  return SendPiReadRequest(session, read_request);
}

absl::Status RemovePiEntities(P4RuntimeSession* session,
                              absl::Span<const Entity* const> entities) {
  WriteRequest clear_request;
  for (const Entity* const entity : entities) {
    Update* update = clear_request.add_updates();
    update->set_type(Update::DELETE);
    *update->mutable_entity() = *entity;
  }
  return SendPiWriteRequest(session, clear_request);
}

absl::Status ClearTableEntries(P4RuntimeSession* session,
                               const IrP4Info& info) {
  ASSIGN_OR_RETURN(ReadResponse table_entries, ReadPiTableEntries(session));
  // Early return if there is nothing to clear.
  if (table_entries.entities_size() == 0) return absl::OkStatus();
  return RemovePiEntities(session, table_entries.entities());
}

absl::Status InstallPiTableEntry(P4RuntimeSession* session,
                                 const p4::v1::TableEntry& pi_entry) {
  WriteRequest request;
  request.set_device_id(session->DeviceId());
  *request.mutable_election_id() = session->ElectionId();

  Update& update = *request.add_updates();
  update.set_type(Update::INSERT);
  *update.mutable_entity()->mutable_table_entry() = pi_entry;

  return SendPiWriteRequest(session, request);
}

}  // namespace pdpi
