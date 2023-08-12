# TODO(haberman): describe this package.

load(
    "//bazel:upb_proto_library.bzl",
    "upb_proto_library",
    "upb_proto_reflection_library",
)

cc_test(
    name = "decode",
    srcs = ["decode.cc"],
    deps = [
        ":struct_upb_proto",
        ":test_upb_proto",
        ":test_upb_proto_reflection",
        "//:upb",
        "//:json",
        "//:reflection",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encode",
    srcs = ["encode.cc"],
    deps = [
        ":struct_upb_proto",
        ":test_upb_proto",
        ":test_upb_proto_reflection",
        "//:upb",
        "//:json",
        "//:reflection",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "test_proto",
    testonly = 1,
    srcs = ["test.proto"],
    deps = ["@com_google_protobuf//:struct_proto"],
)

upb_proto_library(
    name = "test_upb_proto",
    testonly = 1,
    deps = [":test_proto"],
)

upb_proto_reflection_library(
    name = "test_upb_proto_reflection",
    testonly = 1,
    deps = [":test_proto"],
)

# TODO: This target arguably belongs in //google/protobuf/BUILD
upb_proto_library(
    name = "struct_upb_proto",
    testonly = 1,
    deps = ["@com_google_protobuf//:struct_proto"],
)
