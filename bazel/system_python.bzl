# Copyright (c) 2009-2021, Google LLC
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Google LLC nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Google LLC BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Repository rule for using Python 3.x headers from the system."""

# Mock out rules_python's pip.bzl for cases where no system python is found.
_mock_pip = """
def _pip_parse_impl(repository_ctx):
    repository_ctx.file("BUILD.bazel", '''
py_library(
    name = "noop",
    visibility = ["//visibility:public"],
)
''')
    repository_ctx.file("requirements.bzl", '''
def install_deps(*args, **kwargs):
    print("WARNING: could not install pip dependencies")

def requirement(*args, **kwargs):
    return "@{}//:noop"
'''.format(repository_ctx.attr.name))
pip_parse = repository_rule(
    implementation = _pip_parse_impl,
    attrs = {
        "requirements": attr.string(),
        "python_interpreter_target": attr.string(),
    },
)
"""

# Alias rules_python's pip.bzl for cases where a system pythong is found.
_alias_pip = """
load("@rules_python//python:pip.bzl", _pip_parse = "pip_parse")
pip_parse = _pip_parse
"""

_build_file = """
load("@bazel_skylib//lib:selects.bzl", "selects")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@bazel_tools//tools/python:toolchain.bzl", "py_runtime_pair")

cc_library(
   name = "python_headers",
   hdrs = glob(["python/**/*.h"]),
   includes = ["python"],
   visibility = ["//visibility:public"],
)

string_flag(
    name = "internal_python_support",
    build_setting_default = "{support}",
    values = [
        "None",
        "Supported",
        "Unsupported",
    ]
)

config_setting(
    name = "none",
    flag_values = {{
        ":internal_python_support": "None",
    }},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "supported",
    flag_values = {{
        ":internal_python_support": "Supported",
    }},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "unsupported",
    flag_values = {{
        ":internal_python_support": "Unsupported",
    }},
    visibility = ["//visibility:public"],
)

selects.config_setting_group(
    name = "exists",
    match_any = [":supported", ":unsupported"],
    visibility = ["//visibility:public"],
)

sh_binary(
    name = "wrapper",
    srcs = ["wrapper"],
    visibility = ["//visibility:public"],
)

py_runtime(
    name = "py3_runtime",
    interpreter_path = "{interpreter}",
    python_version = "PY3",
)

py_runtime_pair(
    name = "runtime_pair",
    py3_runtime = ":py3_runtime",
)

toolchain(
    name = "python_toolchain",
    toolchain = ":runtime_pair",
    toolchain_type = "@rules_python//python:toolchain_type",
)
"""

_register = """
def register_toolchain():
    native.register_toolchains("@{}//:python_toolchain")
"""

_mock_register = """
def register_toolchain():
    pass
"""

def _get_python_version(repository_ctx):
    py_program = "import sys; print(str(sys.version_info.major) + str(sys.version_info.minor))"
    result = repository_ctx.execute(["python3", "-c", py_program])
    return (result.stdout).strip()

def _get_python_path(repository_ctx):
    py_program = "import sysconfig; print(sysconfig.get_config_var('%s'), end='')"
    result = repository_ctx.execute(["python3", "-c", py_program % ("INCLUDEPY")])
    if result.return_code != 0:
        return None
    return result.stdout

def _populate_package(ctx, path, python3, python_version):
    ctx.symlink(path, "python")
    support = "Supported" if float(python_version) >= 3.7 else "Unsupported"

    build_file = _build_file.format(
        interpreter = python3,
        support = support,
    );

    ctx.file("wrapper", "exec {} \"$@\"".format(python3))
    ctx.file("BUILD.bazel", build_file)
    ctx.file("version.bzl", "SYSTEM_PYTHON_VERSION = '{}'".format(python_version))
    ctx.file("register.bzl", _register.format(ctx.attr.name))
    ctx.file("pip.bzl", """
load("@rules_python//python:pip.bzl", _pip_parse = "pip_parse")
pip_parse = _pip_parse
    """)

def _populate_empty_package(ctx):
    # Mock out all the entrypoints we need to run from WORKSPACE.  Targets that
    # actually need python should use `target_compatible_with` and the generated
    # @system_python//:exists or @system_python//:supported constraints.
    ctx.file("BUILD.bazel",
        _build_file.format(
            interpreter = "",
            support = "None",
        )
    )
    ctx.file("version.bzl", "SYSTEM_PYTHON_VERSION = 'None'")
    ctx.file("register.bzl", _mock_register)
    ctx.file("pip.bzl", _mock_pip)

def _system_python_impl(repository_ctx):
    path = _get_python_path(repository_ctx)
    python3 = repository_ctx.which("python3")
    python_version = _get_python_version(repository_ctx)

    if path and float(python_version) >= 3:
        _populate_package(repository_ctx, path, python3, python_version)
    else:
        # buildifier: disable=print
        print("WARNING: no system python available, builds against system python will fail")
        _populate_empty_package(repository_ctx)


# The system_python() repository rule exposes Python headers from the system.
#
# In WORKSPACE:
#   system_python(
#       name = "system_python_repo",
#   )
#
# This repository exposes a single rule that you can depend on from BUILD:
#   cc_library(
#     name = "foobar",
#     srcs = ["foobar.cc"],
#     deps = ["@system_python_repo//:python_headers"],
#   )
#
# The headers should correspond to the version of python obtained by running
# the `python3` command on the system.
system_python = repository_rule(
    implementation = _system_python_impl,
    attrs = {
        "pip_repo": attr.string(
            doc = "The name of the repository created by pip_parse."
        ),
    },
    local = True,
)

def register_system_python(name):
    native.register_toolchains("@{}//:python_toolchain" % name)
