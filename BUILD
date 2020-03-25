cc_library(
    name = "pdpi",
    srcs = ["pdpi.cc",
            "util.cc"],
    hdrs = ["pdpi.h",
            "util.h"],
    deps = [
      "@com_github_p4lang_p4runtime//:p4info_cc_proto",
      "@com_google_absl//absl/strings:strings",
    ],
)

cc_test(
    name = "pdpi_test",
    size = "small",
    srcs = ["pdpi_test.cc"],
    data = ["testdata"],
    deps = [
        "pdpi",
        "@com_google_googletest//:gtest_main",
    ],
)
