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

#ifndef PDPI_PDPI_H
#define PDPI_PDPI_H

#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"

namespace pdpi {

constexpr char kFieldMatchFieldname[] = "match";
constexpr char kActionFieldname[] = "action";

constexpr char kLpmValueFieldname[] = "value";
constexpr char kLpmPrefixLenFieldname[] = "prefix_len";

constexpr char kTernaryValueFieldname[] = "value";
constexpr char kTernaryMaskFieldname[] = "mask";

void PiTableEntryToPd(const p4::config::v1::P4Info &p4_info,
                      const p4::v1::TableEntry &pi,
                      google::protobuf::Message *pd);
}  // namespace pdpi

#endif  // PDPI_PDPI_H
