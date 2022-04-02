
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_python//python:packaging.bzl", "py_wheel")

def _get_suffix(limited_api, python_version, cpu):
    suffix = "pyd" if ("win" in cpu) else "so"

    if limited_api == False:
        return "." + suffix

    if "win" in cpu:
        if "win32" in cpu:
            abi = "win32"
        elif "win64" in cpu:
            abi = "win_amd64"
        return ".cp{}-{}.{}".format(python_version, abi, suffix)

    abis = {
        "osx-x86_64": "darwin",
        "osx-aarch_64": "darwin",
        "linux-aarch_64": "aarch64-linux-gnu",
        "linux-x86_64": "x86_64-linux-gnu",
        "k8": "x86_64-linux-gnu",
    }

    return ".cpython-{}-{}.{}".format(python_version, abis[cpu], suffix)

def _py_dist_module_impl(ctx):
    base_filename = ctx.attr.module_name.replace(".", "/")
    suffix = _get_suffix(
        limited_api = ctx.attr._limited_api[BuildSettingInfo].value,
        python_version = ctx.attr._python_version[BuildSettingInfo].value,
        cpu = ctx.var["TARGET_CPU"],
    )
    filename = base_filename + suffix
    file = ctx.actions.declare_file(filename)
    src = ctx.attr.extension[DefaultInfo].files.to_list()[0]
    ctx.actions.run(
        executable = "cp",
        arguments = [src.path, file.path],
        inputs = [src],
        outputs = [file],
    )
    return [
        DefaultInfo(files = depset([file])),
    ]


_py_dist_module_rule = rule(
    output_to_genfiles = True,
    implementation = _py_dist_module_impl,
    fragments = ["cpp"],
    attrs = {
        "module_name": attr.string(mandatory = True),
        "extension": attr.label(
            mandatory = True,
            providers = [CcInfo],
        ),
        "_limited_api": attr.label(default = "//python:limited_api"),
        "_python_version": attr.label(default = "//python:python_version"),
        "_cc_toolchain": attr.label(
            default = "@bazel_tools//tools/cpp:current_cc_toolchain",
        ),
    },
)

def py_dist_module(name, module_name, extension):
    file_rule = name + "_file"
    _py_dist_module_rule(
        name = file_rule,
        module_name = module_name,
        extension = extension
    )

    # TODO(haberman): needed?
    native.py_library(
        name = name,
        data = [":" + file_rule],
        imports = ["."],
    )

def py_dist(name, distribution, extension_modules, pure_python_modules, binary_wheels):
    pass
    py_wheel(
        name = "binary_wheel",
        abi = "abi3",
        distribution = distribution,
        # TODO(https://github.com/protocolbuffers/upb/issues/502): we need to make
        # this a select() that is calculated from the platform we are actually
        # building on.
        platform = select({
            ":x86_64_cpu": "manylinux2014_x86_64",
            ":aarch64_cpu": "manylinux2014_aarch64",
            ":win32_cpu": "win32",
            ":win64_cpu": "win_amd64",
        }),
        python_tag = "cp37",
        strip_path_prefixes = ["python/"],
        version = "4.20.0",
    )

# py_dist(
#     name = "dist",
#     distribution = "protobuf",
#     extension_modules = [
#         ":api_implementation_mod",
#         ":message_mod",
#     ],
#     pure_python_modules = [
#         ":well_known_proto_py_pb2",
#         # TODO(https://github.com/protocolbuffers/upb/issues/503): currently
#         # this includes the unit tests.  We should filter these out so we are
#         # only distributing true source files.
#         "@com_google_protobuf//:python_srcs",
#     ],
#     binary_wheels = [
#         # Limited API: these wheels will satisfy any Python version >= the
#         # given version.
#         #
#         # Technically the limited API doesn't have the functions we need until
#         # 3.10, but on Linux we can get away with using 3.7 (see ../python.h for
#         # details).
#         ("3.10", "3.10", "win32"),
#         ("3.10", "3.10", "win64"),
#         ("3.7", "3.7", "linux-x86_64"),
#         ("3.7", "3.7", "linux-aarch_64"),
#         # Windows needs version-specific wheels until 3.10.
#     ] + cross_product(["3.7", "3.8", "3.9"], ["win32", "win64"]),
# )
