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
#include <vector>

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

using ::p4::config::v1::P4Info;
using ::p4::v1::ReadRequest;
using ::p4::v1::ReadResponse;
using ::p4::v1::SetForwardingPipelineConfigRequest;
using ::p4::v1::SetForwardingPipelineConfigResponse;
using ::p4::v1::TableEntry;
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

  grpc::Status reader_status = reader->Finish();
  if (!reader_status.ok()) {
    return gutil::GrpcStatusToAbslStatus(reader_status);
  }
  return response;
}

absl::Status SendPiWriteRequest(P4RuntimeSession* session,
                                const WriteRequest& write_request) {
  grpc::ClientContext context;
  // Empty message; intentionally discarded.
  WriteResponse pi_response;
  return WriteRpcGrpcStatusToAbslStatus(
      session->Stub().Write(&context, write_request, &pi_response),
      write_request.updates_size());
}

absl::StatusOr<std::vector<TableEntry>> ReadPiTableEntries(
    P4RuntimeSession* session) {
  ReadRequest read_request;
  read_request.set_device_id(session->DeviceId());
  read_request.add_entities()->mutable_table_entry();
  ASSIGN_OR_RETURN(ReadResponse read_response,
                   SendPiReadRequest(session, read_request));

  std::vector<TableEntry> table_entries;
  table_entries.reserve(read_response.entities().size());
  for (const auto& entity : read_response.entities()) {
    if (!entity.has_table_entry())
      return gutil::InternalErrorBuilder()
             << "Entity in the read response has no table entry: "
             << entity.DebugString();
    table_entries.push_back(std::move(entity.table_entry()));
  }
  return table_entries;
}

absl::Status ClearTableEntries(P4RuntimeSession* session,
                               const IrP4Info& info) {
  ASSIGN_OR_RETURN(auto table_entries, ReadPiTableEntries(session));
  // Early return if there is nothing to clear.
  if (table_entries.empty()) return absl::OkStatus();
  return RemovePiTableEntries(session, table_entries);
}

absl::Status RemovePiTableEntries(P4RuntimeSession* session,
                                  absl::Span<const TableEntry> pi_entries) {
  WriteRequest clear_request;
  clear_request.set_device_id(session->DeviceId());
  *clear_request.mutable_election_id() = session->ElectionId();

  for (const auto& table_entry : pi_entries) {
    Update* update = clear_request.add_updates();
    update->set_type(Update::DELETE);
    *update->mutable_entity()->mutable_table_entry() = table_entry;
  }
  return SendPiWriteRequest(session, clear_request);
}

absl::Status InstallPiTableEntry(P4RuntimeSession* session,
                                 const TableEntry& pi_entry) {
  return InstallPiTableEntries(session, absl::MakeConstSpan(&pi_entry, 1));
}

absl::Status InstallPiTableEntries(P4RuntimeSession* session,
                                   absl::Span<const TableEntry> pi_entries) {
  WriteRequest batch_write_request;
  batch_write_request.set_device_id(session->DeviceId());
  *batch_write_request.mutable_election_id() = session->ElectionId();

  for (const auto& pi_entry : pi_entries) {
    Update* update = batch_write_request.add_updates();
    update->set_type(Update::INSERT);
    *update->mutable_entity()->mutable_table_entry() = pi_entry;
  }
  return SendPiWriteRequest(session, batch_write_request);
}

absl::Status SetForwardingPipelineConfig(P4RuntimeSession* session,
                                         const P4Info& p4info) {
  SetForwardingPipelineConfigRequest request;
  request.set_device_id(session->DeviceId());
  *request.mutable_election_id() = session->ElectionId();
  request.set_action(SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT);
  *request.mutable_config()->mutable_p4info() = p4info;

  // Empty message; intentionally discarded.
  SetForwardingPipelineConfigResponse response;
  grpc::ClientContext context;
  return gutil::GrpcStatusToAbslStatus(
      session->Stub().SetForwardingPipelineConfig(&context, request,
                                                  &response));
}

}  // namespace pdpi
