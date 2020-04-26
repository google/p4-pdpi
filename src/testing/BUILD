load(":pdpi-check.bzl", "diff_test", "run_pdpi")

# To update expected file, run `bazel run <diff test target> -- --update`.
# Make sure the expected file exists before running --update.
diff_test(
    name = "metadata_test",
    actual = ":metadata",
    expected = "testdata/metadata.expected",
)

run_pdpi(
    name = "metadata",
    src = "testdata/metadata.pb.txt",
)

cc_binary(
    name = "pdpi_test_runner",
    srcs = ["pdpi_test_runner.cc"],
    deps = [
        ":testing_cc_proto",
        "//src:pdpi",
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
        "@com_github_p4lang_p4runtime//:p4info_proto",
    ],
)

cc_proto_library(
    name = "testing_cc_proto",
    deps = [":testing_proto"],
)
