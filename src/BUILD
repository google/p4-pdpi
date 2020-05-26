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

package(
    default_visibility = ["//src:__subpackages__"],
)

cc_library(
    name = "pdpi",
    srcs = [
        "ir.cc",
        "meta.cc",
        "pdpi.cc",
        "util.cc",
    ],
    hdrs = [
        "ir.h",
        "meta.h",
        "pdpi.h",
        "util.h",
    ],
    deps = [
        "ir_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "pdpi_test",
    size = "small",
    srcs = ["pdpi_test.cc"],
    data = ["testdata"],
    deps = [
        "pd_cc_proto",
        "pdpi",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "ir_proto",
    srcs = ["ir.proto"],
)

cc_proto_library(
    name = "ir_cc_proto",
    deps = [":ir_proto"],
)

proto_library(
    name = "p4common_p4_proto",
    srcs = ["testdata/pdpi_p4common.proto"],
)

cc_proto_library(
    name = "p4common_p4_cc_proto",
    deps = [":p4common_p4_proto"],
)

proto_library(
    name = "pd_proto",
    srcs = ["testdata/pdpi_proto_p4.proto"],
    deps = [":p4common_p4_proto"],
)

cc_proto_library(
    name = "pd_cc_proto",
    deps = [":pd_proto"],
)
