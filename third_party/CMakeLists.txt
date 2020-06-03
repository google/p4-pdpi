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
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

set(_PROTOS
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto/p4/config/v1/p4info.proto"
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto/p4/config/v1/p4types.proto")
set(_HDRS
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4info.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4types.pb.h")
set(_SRCS
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4info.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4types.pb.cc")

add_custom_command(
	OUTPUT ${_HDRS} ${_SRCS}
	COMMAND ${_PROTOBUF_PROTOC}
	ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
	     -I "${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto"
	     "${_PROTOS}"
        DEPENDS ${_PROTOS}
	COMMENT "Building P4 Runtime API."
	COMMAND_EXPAND_LISTS)

add_library(p4runtime_proto_cc ${_SRCS})
target_include_directories(p4runtime_proto_cc PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(p4runtime_proto_cc protobuf::libprotobuf)
