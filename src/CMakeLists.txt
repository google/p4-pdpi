# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Create IR protobuf libarary.
set(_P4IR_SRCS "ir.pb.cc")
set(_P4IR_HDRS "ir.pb.h")
protobuf_generate_cpp(${_P4IR_SRCS} ${_P4IR_HDRS} ir.proto)
add_library(pdpi_ir_proto ${_P4IR_SRCS})
target_link_libraries(pdpi_ir_proto
	protobuf::libprotobuf)

# Create IR library.
add_library(pdpi_ir util.cc ir.cc)
target_link_libraries(pdpi_ir
	absl::strings
	pdpi_ir_proto
	p4runtime_proto_grpc)

# Create PDPI aliases for each library to make it clear these libraries
# originate from PDPI.
add_library(pdpi::ir_proto ALIAS pdpi_ir_proto)
add_library(pdpi::ir ALIAS pdpi_ir)
