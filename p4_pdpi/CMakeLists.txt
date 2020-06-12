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

# Generate the source and header files for PDPI's IR proto.
set(_PDPI_IR_PROTO_SRCS "ir.pb.cc")
set(_PDPI_IR_PROTO_HDRS "ir.pb.h")
protobuf_generate_cpp(${_PDPI_IR_PROTO_SRCS} ${_PDPI_IR_PROTO_HDRS} ir.proto)

# Create PDPI IR proto library
add_library(pdpi_ir_proto ${_PDPI_IR_PROTO_SRCS})
target_include_directories(pdpi_ir_proto PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>)
target_link_libraries(pdpi_ir_proto
	p4lang::p4runtime
	protobuf::libprotobuf)
add_library(pdpi::ir_proto ALIAS pdpi_ir_proto)
