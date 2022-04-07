load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

limited_api_build_file = """
cc_library(
    name = "python_headers",
    hdrs = glob(["**/Include/**/*.h"]),
    strip_include_prefix = "Python-{}/Include",
    visibility = ["//visibility:public"],
)
"""

def limited_api_download(version, sha256):
    http_archive(
        name = "python-{}".format(version),
        urls = [
            "https://www.python.org/ftp/python/{0}/Python-{0}.tgz"
            .format(version)],
        sha256 = sha256,
        build_file_content = limited_api_build_file.format(version),
        patch_cmds = [
            "echo '#define SIZEOF_WCHAR_T 4' > Python-{}/Include/pyconfig.h"
            .format(version)]
    )

full_api_build_file = """
cc_import(
    name = "python",
    hdrs = glob(["**/*.h"]),
    shared_library = "python{0}.dll",
    interface_library = "libs/python{0}.lib",
    visibility = ["@upb//python:__pkg__"],
)
"""

def nuget_download(version, cpu, sha256, lib_number):
    folder_name_dict = {
        "i686": "pythonx86",
        "x86-64": "python"
    }

    http_archive(
        name = "nuget_python_{}_{}".format(cpu, version),
        urls = [
            "https://www.nuget.org/api/v2/package/{}/{}"
            .format(folder_name_dict[cpu], version)],
        sha256 = sha256,
        strip_prefix = "tools",
        build_file_content =
            full_api_build_file.format(lib_number),
        type = "zip",
        patch_cmds = ["cp -r include/* ."],
    )


def full_api_download(version, cpu, sha256):
    major_version, minor_version, micro_version = version.split('.')

    nuget_download(
        version = version,
        cpu = cpu,
        sha256 = sha256,
        lib_number = major_version + minor_version,
    )
