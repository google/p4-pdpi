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

load(":pdpi-check.bzl", "diff_test", "run_pdpi")

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
        "testdata/main-p4info.pb.txt",
    ],
)

diff_test(
    name = "ir_test",
    actual = ":ir",
    expected = "testdata/ir.expected",
)

run_pdpi(
    name = "ir",
    src = "testdata/ir.pb.txt",
    deps = [
        "testdata/main-p4info.pb.txt",
    ],
)

cc_binary(
    name = "pdpi_test_runner",
    srcs = ["pdpi_test_runner.cc"],
    deps = [
        ":testing_cc_proto",
        "//p4_pdpi:pdpi",
        "//p4_pdpi/utils:status_utils",
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
