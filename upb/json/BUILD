# TODO(haberman): describe this package.

load("//bazel:build_defs.bzl", "UPB_DEFAULT_COPTS")

cc_library(
    name = "json",
    srcs = [
        "decode.c",
        "encode.c",
    ],
    hdrs = [
        "decode.h",
        "encode.h",
    ],
    copts = UPB_DEFAULT_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        "//:collections",
        "//:lex",
        "//:port",
        "//:reflection",
        "//:wire",
    ],
)
