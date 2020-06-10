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

# Abseil libraries.
add_subdirectory(abseil-cpp)

# The p4lang/p4runtime GitHub repo does not provide CMake build scripts.
# Therefore, we use a custom command to compile the P4Runtime protobufs.
set(_P4_PROTOS
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto/p4/v1/p4data.proto"
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto/p4/v1/p4runtime.proto"
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto/p4/config/v1/p4info.proto"
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto/p4/config/v1/p4types.proto"
	"/usr/share/grpc/third_party/googleapis/google/rpc/status.proto"
	"/usr/share/grpc/third_party/googleapis/google/rpc/code.proto")

set(_P4_HDRS
	# cpp protos
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4data.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4runtime.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4info.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4types.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/status.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/code.pb.h"

	# gRPC protos
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4data.grpc.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4runtime.grpc.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4info.grpc.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4types.grpc.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/status.grpc.pb.h"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/code.grpc.pb.h")

set(_P4_SRCS
	# cpp protos
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4data.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4runtime.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4info.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4types.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/status.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/code.pb.cc"

	# gRPC protos
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4data.grpc.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/v1/p4runtime.grpc.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4info.grpc.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/p4/config/v1/p4types.grpc.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/status.grpc.pb.cc"
	"${CMAKE_CURRENT_BINARY_DIR}/google/rpc/code.grpc.pb.cc")

# Add P4Runtime protos to the protobuf import directory so other protos can
# use them.
set(Protobuf_IMPORT_DIRS
	"${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto"
	PARENT_SCOPE)

add_custom_command(
	OUTPUT ${_P4_HDRS} ${_P4_SRCS}
	COMMAND ${_PROTOBUF_PROTOC}
	ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
	     --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
	     -I "${CMAKE_CURRENT_SOURCE_DIR}/p4runtime/proto"
	     -I "/usr/share/grpc/third_party/googleapis"
	     --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
	     "${_P4_PROTOS}"
        DEPENDS ${_P4_PROTOS}
	COMMENT "Building P4 Runtime API."
	COMMAND_EXPAND_LISTS)

add_library(p4lang_p4runtime_proto ${_P4_SRCS})
target_include_directories(p4lang_p4runtime_proto PUBLIC
	"${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(p4lang_p4runtime_proto
	gRPC::grpc++
	gRPC::grpc++_reflection
	protobuf::libprotobuf)
add_library(p4lang::p4runtime ALIAS p4lang_p4runtime_proto)
