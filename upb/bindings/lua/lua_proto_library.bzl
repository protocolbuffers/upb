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

"""lua_proto_library(): a rule for building Lua protos."""

load("@rules_proto//proto:defs.bzl", "proto_common")

# Generic support code #########################################################

# upb_proto_library / upb_proto_reflection_library shared code #################

_LuaFilesInfo = provider(
    "A set of lua files generated from .proto files",
    fields = ["files"],
)

def _compile_upb_protos(ctx, proto_info, proto_sources):
    files = proto_common.declare_generated_files(ctx.actions, proto_info, extension = "_pb.lua")
    transitive_sets = proto_info.transitive_descriptor_sets.to_list()
    proto_common.compile(
        ctx.actions,
        proto_info,
        ctx.attr._lua_proto_toolchain[proto_common.ProtoLangToolchainInfo],
        generated_files = files,
        plugin_output = proto_info.proto_source_root if proto_info.proto_source_root != '.' else ctx.bin_dir.path,
    )
    return files

def _lua_proto_rule_impl(ctx):
    if len(ctx.attr.deps) != 1:
        fail("only one deps dependency allowed.")
    dep = ctx.attr.deps[0]
    if _LuaFilesInfo not in dep:
        fail("proto_library rule must generate _LuaFilesInfo (aspect should have handled this).")
    files = dep[_LuaFilesInfo].files
    return [
        DefaultInfo(
            files = files,
            data_runfiles = ctx.runfiles(files = files.to_list()),
        ),
    ]

def _lua_proto_library_aspect_impl(target, ctx):
    proto_info = target[ProtoInfo]
    files = _compile_upb_protos(ctx, proto_info, proto_info.direct_sources)
    deps = ctx.rule.attr.deps
    transitive = [dep[_LuaFilesInfo].files for dep in deps if _LuaFilesInfo in dep]
    return [_LuaFilesInfo(files = depset(direct = files, transitive = transitive))]

# lua_proto_library() ##########################################################

_lua_proto_library_aspect = aspect(
    attrs = {
        "_lua_proto_toolchain": attr.label(
            default = "//upb/bindings/lua:lua_proto_toolchain",
        ),
    },
    implementation = _lua_proto_library_aspect_impl,
    provides = [_LuaFilesInfo],
    attr_aspects = ["deps"],
)

lua_proto_library = rule(
    output_to_genfiles = True,
    implementation = _lua_proto_rule_impl,
    attrs = {
        "deps": attr.label_list(
            aspects = [_lua_proto_library_aspect],
            allow_rules = ["proto_library"],
            providers = [ProtoInfo],
        ),
    },
)
