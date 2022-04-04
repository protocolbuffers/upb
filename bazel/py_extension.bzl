load(
    "//bazel:build_defs.bzl",
    "UPB_DEFAULT_COPTS",
)

def py_extension(name, srcs, deps = []):
    version_script = name + "_version_script.lds"
    symbol = "PyInit_" + name
    native.genrule(
        name = "gen_" + version_script,
        outs = [version_script],
        cmd = "echo 'message { global: " + symbol + "; local: *; };' > $@",
    )

    LIMITED_API_FLAG_SELECT = {
        ":limited_api_3.7": ["-DPy_LIMITED_API=0x03070000"],
        ":limited_api_3.10": ["-DPy_LIMITED_API=0x030a0000"],
        "//conditions:default": [],
    }

    native.cc_binary(
        name = name,
        srcs = srcs,
        copts = UPB_DEFAULT_COPTS + select(LIMITED_API_FLAG_SELECT) + [
            # The Python API requires patterns that are ISO C incompatible, like
            # casts between function pointers and object pointers.
            "-Wno-pedantic",
            # "--compat-implib",
        ],
        linkshared = True,
        linkstatic = True,
        deps = deps + [
            ":" + version_script
        ] + select({
            "//python:limited_api_3.7": ["@python-3.7.0//:python_headers"],
            "//python:full_api_3.7_win32": ["@nuget_python_i686_3.7.0//:python"],
            "//python:full_api_3.7_win64": ["@nuget_python_x86-64_3.7.0//:python"],
            "//python:full_api_3.8_win32": ["@nuget_python_i686_3.8.0//:python"],
            "//python:full_api_3.8_win64": ["@nuget_python_x86-64_3.8.0//:python"],
            "//python:full_api_3.9_win32": ["@nuget_python_i686_3.9.0//:python"],
            "//python:full_api_3.9_win64": ["@nuget_python_x86-64_3.9.0//:python"],
            "//python:limited_api_3.10_win32": ["@nuget_python_i686_3.10.0//:python"],
            "//python:limited_api_3.10_win64": ["@nuget_python_x86-64_3.10.0//:python"],
            "//conditions:default": ["@system_python//:python_headers"],
        }),
    )
