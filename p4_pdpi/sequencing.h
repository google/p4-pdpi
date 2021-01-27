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

#ifndef GOOGLE_P4_PDPI_SEQUENCING_H_
#define GOOGLE_P4_PDPI_SEQUENCING_H_

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

// Returns a list of write requests, such that updates are sequenced correctly
// when the write requests are sent in order. See go/p4-sequencing for details.
std::vector<p4::v1::WriteRequest> SequenceP4Updates(
    const IrP4Info& info, const std::vector<p4::v1::Update>& updates);

}  // namespace pdpi

#endif  // GOOGLE_P4_PDPI_SEQUENCING_H_
