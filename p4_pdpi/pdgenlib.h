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

#ifndef P4_PDPI_PD_H_
#define P4_PDPI_PD_H_

#include "gutil/status.h"
#include "p4_pdpi/ir.pb.h"

namespace pdpi {

// Returns the PD proto definition for the given P4 info. May not be fully
// formatted according to any style guide.
gutil::StatusOr<std::string> IrP4InfoToPdProto(const IrP4Info& info);

}  // namespace pdpi

#endif  // P4_PDPI_PD_H_
