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

# Some PDPI protobufs rely on P4Runtime protobufs which therefore need to be
# compiled before they can be used.
set(Protobuf_IMPORT_DIRS
	"${CMAKE_CURRENT_SOURCE_DIR}/third_party/p4runtime/proto")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/third_party")
add_subdirectory(third_party)

# CMake is used to export libraries related to the P4 IR. It currently cannot
# be used to build the entire PDPI project.
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(src)
