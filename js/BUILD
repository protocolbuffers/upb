load("//third_party/bazel_rules/rules_wasm/wasm:defs.bzl", "wasm_cc_binary")
load("//testing/karma/builddefs:karma_web_test_suite.bzl", "karma_web_test_suite")
load("//tools/build_defs/js:rules.bzl", "js_library")

cc_binary(
    name = "upb_js_c",
    srcs = ["upb_js.c"],
    linkopts = select({
        "//tools/cc_target_os:emscripten": [
            "-sEXPORTED_FUNCTIONS=_malloc,_upb_Arena_New,_upb_Arena_Free",
            "-sFILESYSTEM=0",
        ],
        "//conditions:default": [],
    }),
    deps = [
        "//:mem",
    ],
)

wasm_cc_binary(
    name = "upb_js_wasm",
    cc_target = ":upb_js_c",
)

js_library(
    name = "upb_js_test_lib",
    srcs = ["upb_js_test.js"],
    check_level = "OFF",
    data = [":upb_js_wasm"],
)

karma_web_test_suite(
    name = "upb_js_tests",
    browsers = [
        "//testing/web/browsers:chrome-linux",
    ],
    concatjs = 0,
    config_file = ":wasm_karma.conf.js",
    data = ["upb_js_test_dom.html"],
    target_compatible_with = select({
        "//tools/target_cpu:k8": [],
        "//conditions:default": ["//third_party/bazel_platforms:incompatible"],
    }),
    deps = [":upb_js_test_lib"],
)
