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

load("@rules_proto//proto:defs.bzl", "proto_library")
load(":pdpi-check.bzl", "diff_test", "run_pdpi")
load("//p4_pdpi:pdgen.bzl", "p4_pd_proto")
load("@com_github_p4lang_p4c//:bazel/p4_library.bzl", "p4_library")

p4_library(
    name = "main_p4info",
    src = "testdata/main.p4",
    p4info_out = "main-p4info.pb.txt",
)

p4_pd_proto(
    name = "main_p4_pd",
    src = "testdata/main.p4",
    out = "main_p4_pd.proto",
)

# TODO(heule): enable once the clang-format version problem has been fixed.
#diff_test(
#    name = "main_pd_test",
#    actual = ":main_p4_pd",
#    expected = "testdata/main_p4_pd.expected",
#)

# To update expected file, run `bazel run <diff test target> -- --update`.
# Make sure the expected file exists before running --update.
diff_test(
    name = "info_test",
    actual = ":info",
    expected = "testdata/info.expected",
)

run_pdpi(
    name = "info",
    src = "testdata/info.pb.txt",
    deps = [
        ":main_p4info",
    ],
)

diff_test(
    name = "table_entry_test",
    actual = ":table_entry",
    expected = "testdata/table_entry.expected",
)

run_pdpi(
    name = "table_entry",
    src = "testdata/table_entry.pb.txt",
)

diff_test(
    name = "packet_io_test",
    actual = ":packet_io",
    expected = "testdata/packet_io.expected",
)

run_pdpi(
    name = "packet_io",
    src = "testdata/packet_io.pb.txt",
)

cc_binary(
    name = "pdpi_test_runner",
    srcs = ["pdpi_test_runner.cc"],
    data = [
        "main-p4info.pb.txt",
    ],
    deps = [
        ":testing_cc_proto",
        "//gutil:proto",
        "//gutil:status",
        "//p4_pdpi:ir",
        "//p4_pdpi:pd",
        "@bazel_tools//tools/cpp/runfiles",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/strings",
    ],
)

proto_library(
    name = "testing_proto",
    srcs = ["testing.proto"],
    deps = [
        "//p4_pdpi:ir_proto",
        "@com_github_p4lang_p4runtime//:p4info_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_proto",
    ],
)

cc_proto_library(
    name = "testing_cc_proto",
    deps = [":testing_proto"],
)
