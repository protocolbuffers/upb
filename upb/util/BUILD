load(
    "//bazel:upb_proto_library.bzl",
    "upb_proto_library",
    "upb_proto_reflection_library",
)

# begin:google_only
# package(default_applicable_licenses = ["//:license"])
# end:google_only

licenses(["notice"])

# Def to Proto

cc_library(
    name = "def_to_proto",
    srcs = ["def_to_proto.c"],
    hdrs = ["def_to_proto.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//:port",
        "//:reflection",
        "//:reflection_internal",
    ],
)

proto_library(
    name = "def_to_proto_test_proto",
    srcs = [
        "def_to_proto_public_import_test.proto",
        "def_to_proto_regular_import_test.proto",
        "def_to_proto_test.proto",
    ],
)

upb_proto_library(
    name = "def_to_proto_test_upb_proto",
    deps = ["def_to_proto_test_proto"],
)

upb_proto_reflection_library(
    name = "def_to_proto_test_upb_proto_reflection",
    deps = ["def_to_proto_test_proto"],
)

cc_library(
    name = "def_to_proto_test_lib",
    testonly = 1,
    hdrs = ["def_to_proto_test.h"],
    deps = [
        ":def_to_proto",
        "//:base",
        "//:descriptor_upb_proto",
        "//:mem",
        "//:reflection_internal",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "def_to_proto_test",
    srcs = ["def_to_proto_test.cc"],
    deps = [
        ":def_to_proto",
        ":def_to_proto_test_lib",
        ":def_to_proto_test_upb_proto",
        ":def_to_proto_test_upb_proto_reflection",
        "//:descriptor_upb_proto_reflection",
        "//:mem",
        "//:reflection",
        "//upb/test:parse_text_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

# begin:google_only
# cc_test(
#     name = "def_to_proto_fuzz_test",
#     srcs = ["def_to_proto_fuzz_test.cc"],
#     tags = ["clang_only"],
#     deps = [
#         ":def_to_proto_test_lib",
#         "//testing/fuzzing:fuzztest",
#         "@com_google_googletest//:gtest_main",
#     ],
# )
# end:google_only

# Required fields

cc_library(
    name = "required_fields",
    srcs = ["required_fields.c"],
    hdrs = ["required_fields.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//:collections",
        "//:port",
        "//:reflection",
    ],
)

proto_library(
    name = "required_fields_test_proto",
    srcs = ["required_fields_test.proto"],
)

upb_proto_library(
    name = "required_fields_test_upb_proto",
    deps = ["required_fields_test_proto"],
)

upb_proto_reflection_library(
    name = "required_fields_test_upb_proto_reflection",
    deps = ["required_fields_test_proto"],
)

cc_test(
    name = "required_fields_test",
    srcs = ["required_fields_test.cc"],
    deps = [
        ":required_fields",
        ":required_fields_test_upb_proto",
        ":required_fields_test_upb_proto_reflection",
        "//:base",
        "//:json",
        "//:mem",
        "//:reflection",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

# Compare

cc_library(
    name = "compare",
    srcs = ["compare.c"],
    hdrs = ["compare.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//:base",
        "//:eps_copy_input_stream",
        "//:port",
        "//:wire_reader",
        "//:wire_types",
    ],
)

cc_test(
    name = "compare_test",
    srcs = ["compare_test.cc"],
    deps = [
        ":compare",
        "//:wire_internal",
        "//:wire_types",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

# begin:github_only
filegroup(
    name = "source_files",
    srcs = [
        "compare.c",
        "compare.h",
        "def_to_proto.c",
        "def_to_proto.h",
        "required_fields.c",
        "required_fields.h",
    ],
    visibility = ["//python/dist:__pkg__"],
)
# end:github_only
