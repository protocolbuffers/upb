#!/usr/bin/python
#
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

"""A tool to convert {WORKSPACE, BUILD} -> CMakeLists.txt.

This tool is very upb-specific at the moment, and should not be seen as a
generic Bazel -> CMake converter.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import sys
import textwrap
import os

_stages = ["_stage0", "_stage1", ""]
_block_targets = ["libupb.so", "libupbc.so", "upbdev", "protoc-gen-upbdev"]
_special_targets_mapping = {
  "com_google_protobuf//:protobuf" : "protobuf::libprotobuf",
  "com_google_protobuf//src/google/protobuf/compiler:code_generator" : "protobuf::libprotoc",
}

def MappingThirdPartyDep(dep):
  if dep.startswith("/:"):
    return dep[2:]
  if dep in _special_targets_mapping:
    return _special_targets_mapping[dep]
  if dep.startswith("com_google_absl//"):
    p = dep.rfind(":")
    if p < 0:
      return "absl::" + dep[dep.rfind("/")+1:]
    return "absl::" + dep[p+1:]
  p = dep.rfind(":")
  if p > 0:
    return dep[p+1:]
  p = dep.rfind("/")
  if p > 0:
    return dep[p+1:]
  return dep

def StripFirstChar(deps):
  return [MappingThirdPartyDep(dep[1:]) for dep in deps]

def IsSourceFile(name):
  return name.endswith(".c") or name.endswith(".cc")


ADD_LIBRARY_FORMAT = """
add_library(%(name)s %(type)s
    %(sources)s
)
target_include_directories(%(name)s %(keyword)s
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(%(name)s %(keyword)s
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
"""

ADD_EXECUTABLE_FORMAT = """
add_executable(%(name)s
    %(sources)s
)
target_include_directories(%(name)s %(keyword)s
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(%(name)s %(keyword)s
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
"""

class BuildFileFunctions(object):
  def __init__(self, converter, subdir):
    self.converter = converter
    self.subdir = subdir

  def _add_deps(self, kwargs, keyword="", stage = ""):
    if "deps" not in kwargs:
      return
    self.converter.toplevel += "target_link_libraries(%s%s\n  %s)\n" % (
        kwargs["name"] + stage,
        keyword,
        "\n  ".join(StripFirstChar(kwargs["deps"]))
    )

  def _add_bootstrap_deps(self, kwargs, keyword="", stage = "", deps_key = "bootstrap_deps"):
    if deps_key not in kwargs:
      return
    self.converter.toplevel += "target_link_libraries({0}{1}\n  {2})\n".format(
        kwargs["name"] + stage,
        keyword,
        "\n  ".join([dep + stage for dep in StripFirstChar(kwargs[deps_key])])
    )


  def load(self, *args):
    pass
  
  def cc_library(self, **kwargs):
    if kwargs["name"].endswith("amalgamation"):
      return
    if kwargs["name"] in _block_targets:
      return
    if "testonly" in kwargs:
      return
    files = kwargs.get("srcs", []) + kwargs.get("hdrs", [])
    found_files = []
    pregenerated_files = [
        "CMakeLists.txt", "descriptor.upb.h", "descriptor.upb.c"
    ]
    for file in files:
      if os.path.basename(file) in pregenerated_files:
        found_files.append("../cmake/" + self.subdir + file)
      else:
        found_files.append("../" + self.subdir + file)

    if list(filter(IsSourceFile, files)):
      # Has sources, make this a normal library.
      self.converter.toplevel += ADD_LIBRARY_FORMAT % {
          "name": kwargs["name"],
          "type": "",
          "keyword": "PUBLIC",
          "sources": "\n  ".join(found_files),
      }
      self.converter.export_targets.append(kwargs["name"])
      self._add_deps(kwargs, " PUBLIC")
    else:
      # Header-only library, have to do a couple things differently.
      # For some info, see:
      #  http://mariobadr.com/creating-a-header-only-library-with-cmake.html
      self.converter.toplevel += ADD_LIBRARY_FORMAT % {
          "name": kwargs["name"],
          "type": "INTERFACE",
          "keyword": "INTERFACE",
          "sources": "",
      }
      self.converter.export_targets.append(kwargs["name"])
      self._add_deps(kwargs, " INTERFACE")

  def cc_binary(self, **kwargs):
    if kwargs["name"] in _block_targets:
      return

    files = kwargs.get("srcs", []) + kwargs.get("hdrs", [])
    found_files = []
    for file in files:
      found_files.append("../" + self.subdir + file)

    self.converter.toplevel += "if (UPB_ENABLE_CODEGEN)\n"
    self.converter.toplevel += ADD_EXECUTABLE_FORMAT % {
        "name": kwargs["name"],
        "keyword": "PRIVATE",
        "sources": "\n  ".join(found_files),
    }
    self.converter.export_codegen_targets.append(kwargs["name"])
    self._add_deps(kwargs, " PRIVATE")
    self.converter.toplevel += "endif()\n"

  def cc_test(self, **kwargs):
    # Disable this until we properly support upb_proto_library().
    # self.converter.toplevel += "add_executable(%s\n  %s)\n" % (
    #     kwargs["name"],
    #     "\n  ".join(kwargs["srcs"])
    # )
    # self.converter.toplevel += "add_test(NAME %s COMMAND %s)\n" % (
    #     kwargs["name"],
    #     kwargs["name"],
    # )

    # if "data" in kwargs:
    #   for data_dep in kwargs["data"]:
    #     self.converter.toplevel += textwrap.dedent("""\
    #       add_custom_command(
    #           TARGET %s POST_BUILD
    #           COMMAND ${CMAKE_COMMAND} -E copy
    #                   ${CMAKE_SOURCE_DIR}/%s
    #                   ${CMAKE_CURRENT_BINARY_DIR}/%s)\n""" % (
    #       kwargs["name"], data_dep, data_dep
    #     ))

    # self._add_deps(kwargs)
    pass

  def cc_fuzz_test(self, **kwargs):
    pass

  def pkg_files(self, **kwargs):
    pass

  def py_library(self, **kwargs):
    pass

  def py_binary(self, **kwargs):
    pass

  def lua_proto_library(self, **kwargs):
    pass

  def sh_test(self, **kwargs):
    pass

  def make_shell_script(self, **kwargs):
    pass

  def exports_files(self, files, **kwargs):
    pass

  def proto_library(self, **kwargs):
    pass

  def cc_proto_library(self, **kwargs):
    pass

  def staleness_test(self, **kwargs):
    pass

  def upb_amalgamation(self, **kwargs):
    pass

  def upb_proto_library(self, **kwargs):
    pass

  def upb_proto_library_copts(self, **kwargs):
    pass

  def upb_proto_reflection_library(self, **kwargs):
    pass

  def upb_proto_srcs(self, **kwargs):
    pass

  def genrule(self, **kwargs):
    pass

  def config_setting(self, **kwargs):
    pass

  def upb_fasttable_enabled(self, **kwargs):
    pass

  def select(self, arg_dict):
    return []

  def glob(self, *args, **kwargs):
    return []

  def licenses(self, *args):
    pass

  def filegroup(self, **kwargs):
    pass

  def map_dep(self, arg):
    return arg

  def package_group(self, **kwargs):
    pass

  def bool_flag(self, **kwargs):
    pass

  def bootstrap_upb_proto_library(self, **kwargs):
    if kwargs["name"] in _block_targets:
      return
    if "oss_src_files" not in kwargs:
      return
    oss_src_files = kwargs["oss_src_files"]
    if not oss_src_files:
      return
    if "base_dir" not in kwargs:
      base_dir = self.subdir
    else:
      base_dir = self.subdir + kwargs["base_dir"]
    while base_dir.endswith("/") or base_dir.endswith("\\"):
      base_dir = base_dir[0:-1]

    oss_src_files_prefix = [".".join(x.split(".")[0:-1]) for x in oss_src_files]
    self.converter.toplevel += "if (UPB_ENABLE_CODEGEN)\n"
    # Stage0
    self.converter.toplevel += ADD_LIBRARY_FORMAT % {
        "name": kwargs["name"] + _stages[0],
        "type": "",
        "keyword": "PUBLIC",
        "sources": "\n  ".join(["../{0}/stage0/{1}.upb.h\n  ../{0}/stage0/{1}.upb.c".format(base_dir, x) for x in oss_src_files_prefix]),
    }
    self.converter.toplevel += "target_include_directories({0}\n".format(kwargs["name"] + _stages[0])
    self.converter.toplevel += "  BEFORE PUBLIC \"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../" + "{0}/stage0>\")\n".format(base_dir)
    self.converter.toplevel += "target_link_libraries({0} PUBLIC\n".format(kwargs["name"] + _stages[0])
    self.converter.toplevel += "  generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me\n"
    self.converter.toplevel += "  mini_table)\n".format(kwargs["name"] + _stages[0])
    self._add_bootstrap_deps(kwargs, " PUBLIC", _stages[0], "deps")

    # Stage1
    stage1_generated_dir = "${CMAKE_CURRENT_BINARY_DIR}/" + _stages[1] + "/" + kwargs["name"]
    self.converter.toplevel += "file(MAKE_DIRECTORY \"{0}\")\n".format(stage1_generated_dir)
    self.converter.toplevel += "add_custom_command(\n"
    self.converter.toplevel += "  OUTPUT\n    {0}\n".format(
      "\n    ".join(["{0}/{1}.upb.h\n    {0}/{1}.upb.c".format(stage1_generated_dir, x) for x in oss_src_files_prefix])
    )
    self.converter.toplevel += "  DEPENDS\n    {0}\n".format(
      "\n    ".join(["{0}/{1}".format("${UPB_HOST_INCLUDE_DIR}", x) for x in oss_src_files])
    )
    self.converter.toplevel += "  COMMAND\n"
    self.converter.toplevel += "    \"${PROTOC_PROGRAM}\"\n    \"-I${UPB_HOST_INCLUDE_DIR}\"\n"
    self.converter.toplevel += "    \"--plugin=protoc-gen-upb=\\$<TARGET_FILE:protoc-gen-upb_stage0>\"\n"
    self.converter.toplevel += "    \"--upb_out={0}\"\n".format(stage1_generated_dir)
    self.converter.toplevel += "    {0}\n".format(
      "\n    ".join(["{0}/{1}".format("${UPB_HOST_INCLUDE_DIR}", x) for x in oss_src_files])
    )
    self.converter.toplevel += ")\n"

    self.converter.toplevel += ADD_LIBRARY_FORMAT % {
        "name": kwargs["name"] + _stages[1],
        "type": "",
        "keyword": "PUBLIC",
        "sources": "\n  ".join(["{0}/{1}.upb.h\n  {0}/{1}.upb.c".format(stage1_generated_dir, x) for x in oss_src_files_prefix]),
    }
    self.converter.toplevel += "target_include_directories({0}\n".format(kwargs["name"] + _stages[1])
    self.converter.toplevel += "  BEFORE PUBLIC \"$<BUILD_INTERFACE:{0}>\")\n".format(stage1_generated_dir)
    self.converter.toplevel += "target_link_libraries({0} PUBLIC\n".format(kwargs["name"] + _stages[1])
    self.converter.toplevel += "  generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me\n"
    self.converter.toplevel += ")\n".format(kwargs["name"] + _stages[1])
    self._add_bootstrap_deps(kwargs, " PUBLIC", _stages[1], "deps")

    # Stage2
    stage2_generated_dir = "${CMAKE_CURRENT_BINARY_DIR}/stage2/" + kwargs["name"]
    self.converter.toplevel += "file(MAKE_DIRECTORY \"{0}\")\n".format(stage2_generated_dir)
    self.converter.toplevel += "add_custom_command(\n"
    self.converter.toplevel += "  OUTPUT\n    {0}\n".format(
      "\n    ".join([
        "\n    ".join(["{0}/{1}.upb.h\n    {0}/{1}.upb.c".format(stage2_generated_dir, x) for x in oss_src_files_prefix]),
      ])
    )
    self.converter.toplevel += "  DEPENDS\n    {0}\n".format(
      "\n    ".join(["{0}/{1}".format("${UPB_HOST_INCLUDE_DIR}", x) for x in oss_src_files])
    )
    self.converter.toplevel += "  COMMAND\n"
    self.converter.toplevel += "    \"${PROTOC_PROGRAM}\"\n    \"-I${UPB_HOST_INCLUDE_DIR}\"\n"
    self.converter.toplevel += "    \"--plugin=protoc-gen-upb=\\$<TARGET_FILE:protoc-gen-upb_stage1>\"\n"
    self.converter.toplevel += "    \"--upb_out={0}\"\n".format(stage2_generated_dir)
    self.converter.toplevel += "    {0}\n".format(
      "\n    ".join(["{0}/{1}".format("${UPB_HOST_INCLUDE_DIR}", x) for x in oss_src_files])
    )
    self.converter.toplevel += ")\n"

    self.converter.toplevel += "add_custom_command(\n"
    self.converter.toplevel += "  OUTPUT\n    {0}\n".format(
      "\n    ".join([
        "\n    ".join(["{0}/{1}.upbdefs.h\n    {0}/{1}.upbdefs.c".format(stage2_generated_dir, x) for x in oss_src_files_prefix]),
        "\n    ".join(["{0}/{1}_pb.lua".format(stage2_generated_dir, x) for x in oss_src_files_prefix])
      ])
    )
    self.converter.toplevel += "  DEPENDS\n    {0}\n".format(
      "\n    ".join(["{0}/{1}".format("${UPB_HOST_INCLUDE_DIR}", x) for x in oss_src_files])
    )
    self.converter.toplevel += "  COMMAND\n"
    self.converter.toplevel += "    \"${PROTOC_PROGRAM}\"\n    \"-I${UPB_HOST_INCLUDE_DIR}\"\n"
    self.converter.toplevel += "    \"--plugin=protoc-gen-upbdefs=\\$<TARGET_FILE:protoc-gen-upbdefs>\"\n"
    self.converter.toplevel += "    \"--plugin=protoc-gen-lua=\\$<TARGET_FILE:protoc-gen-lua>\"\n"
    self.converter.toplevel += "    \"--upbdefs_out={0}\"\n".format(stage2_generated_dir)
    self.converter.toplevel += "    \"--lua_out={0}\"\n".format(stage2_generated_dir)
    self.converter.toplevel += "    {0}\n".format(
      "\n    ".join(["{0}/{1}".format("${UPB_HOST_INCLUDE_DIR}", x) for x in oss_src_files])
    )
    self.converter.toplevel += ")\n"

    self.converter.toplevel += ADD_LIBRARY_FORMAT % {
        "name": kwargs["name"] + _stages[2],
        "type": "",
        "keyword": "PUBLIC",
        "sources": "\n  ".join(["{0}/{1}.upb.h\n  {0}/{1}.upb.c".format(stage2_generated_dir, x) for x in oss_src_files_prefix]),
    }
    self.converter.toplevel += "target_include_directories({0}\n".format(kwargs["name"] + _stages[2])
    self.converter.toplevel += "  BEFORE PUBLIC \"$<BUILD_INTERFACE:{0}>\")\n".format(stage2_generated_dir)
    self.converter.toplevel += "target_link_libraries({0} PUBLIC\n".format(kwargs["name"] + _stages[2])
    self.converter.toplevel += "  upb\n"
    self.converter.toplevel += ")\n".format(kwargs["name"] + _stages[2])
    self._add_bootstrap_deps(kwargs, " PUBLIC", _stages[2], "deps")

    self.converter.toplevel += ADD_LIBRARY_FORMAT % {
        "name": kwargs["name"] + _stages[2] + "_defs",
        "type": "",
        "keyword": "PUBLIC",
        "sources": "\n  ".join(["{0}/{1}.upbdefs.h\n  {0}/{1}.upbdefs.c".format(stage2_generated_dir, x) for x in oss_src_files_prefix]),
    }
    self.converter.toplevel += "target_include_directories({0}\n".format(kwargs["name"] + _stages[2] + "_defs")
    self.converter.toplevel += "  BEFORE PUBLIC \"$<BUILD_INTERFACE:{0}>\")\n".format(stage2_generated_dir)
    self.converter.toplevel += "target_link_libraries({0} PUBLIC\n".format(kwargs["name"] + _stages[2] + "_defs")
    self.converter.toplevel += "  {0}\n".format(kwargs["name"] + _stages[2])
    self.converter.toplevel += ")\n".format(kwargs["name"] + _stages[2])

    self.converter.toplevel += "install(\n"
    self.converter.toplevel += "  FILES\n    {0}\n".format(
      "\n    ".join([
        "\n    ".join(["{0}/{1}.upb.h\n    {0}/{1}.upb.c".format(stage2_generated_dir, x) for x in oss_src_files_prefix]),
        "\n    ".join(["{0}/{1}.upbdefs.h\n    {0}/{1}.upbdefs.c".format(stage2_generated_dir, x) for x in oss_src_files_prefix]),
        "\n    ".join(["{0}/{1}_pb.lua".format(stage2_generated_dir, x) for x in oss_src_files_prefix])
      ])
    )
    self.converter.toplevel += "  DESTINATION \"include/{0}\"\n".format(os.path.dirname(oss_src_files_prefix[0]))
    self.converter.toplevel += ")\n"

    self.converter.export_codegen_targets.append(kwargs["name"] + _stages[2])
    self.converter.export_codegen_targets.append(kwargs["name"] + _stages[2] + "_defs")

    self.converter.toplevel += "endif()\n"

  def bootstrap_cc_library(self, **kwargs):
    if kwargs["name"] in _block_targets:
      return
    files = kwargs.get("srcs", []) + kwargs.get("hdrs", [])
    found_files = []
    for file in files:
      found_files.append("../" + self.subdir + file)

    self.converter.toplevel += "if (UPB_ENABLE_CODEGEN)\n"
    for stage in _stages:
      stage_name = kwargs["name"] + stage
      if list(filter(IsSourceFile, files)):
        # Has sources, make this a normal library.
        self.converter.toplevel += ADD_LIBRARY_FORMAT % {
            "name": stage_name,
            "type": "",
            "keyword": "PUBLIC",
            "sources": "\n  ".join(found_files),
        }
        self._add_deps(kwargs, " PUBLIC", stage)
        self._add_bootstrap_deps(kwargs, " PUBLIC", stage)
      else:
        # Header-only library, have to do a couple things differently.
        # For some info, see:
        #  http://mariobadr.com/creating-a-header-only-library-with-cmake.html
        self.converter.toplevel += ADD_LIBRARY_FORMAT % {
            "name": stage_name,
            "type": "INTERFACE",
            "keyword": "INTERFACE",
            "sources": "",
        }
        self._add_deps(kwargs, " INTERFACE", stage)
        self._add_bootstrap_deps(kwargs, " INTERFACE", stage)
    self.converter.export_codegen_targets.append(kwargs["name"])
    self.converter.toplevel += "endif()\n"

  def bootstrap_cc_binary(self, **kwargs):
    if kwargs["name"] in _block_targets:
      return
    files = kwargs.get("srcs", []) + kwargs.get("hdrs", [])
    found_files = []
    for file in files:
      found_files.append("../" + self.subdir + file)

    # Has sources, make this a normal library.
    self.converter.toplevel += "if (UPB_ENABLE_CODEGEN)\n"
    for stage in _stages:
      stage_name = kwargs["name"] + stage
      self.converter.toplevel += ADD_EXECUTABLE_FORMAT % {
          "name": stage_name,
          "keyword": "PRIVATE",
          "sources": "\n  ".join(found_files),
      }
      self._add_deps(kwargs, " PRIVATE", stage)
      self._add_bootstrap_deps(kwargs, " PRIVATE", stage)
    self.converter.export_codegen_targets.append(kwargs["name"])
    self.converter.toplevel += "endif()\n"

class WorkspaceFileFunctions(object):
  def __init__(self, converter):
    self.converter = converter

  def load(self, *args, **kwargs):
    pass

  def workspace(self, **kwargs):
    self.converter.prelude += "project(%s)\n" % (kwargs["name"])
    self.converter.prelude += "set(CMAKE_C_STANDARD 99)\n"

  def maybe(self, rule, **kwargs):
    if kwargs["name"] == "utf8_range":
      self.converter.utf8_range_commit = kwargs["commit"]
    pass

  def http_archive(self, **kwargs):
    pass

  def git_repository(self, **kwargs):
    pass

  def new_git_repository(self, **kwargs):
    pass

  def bazel_version_repository(self, **kwargs):
    pass

  def protobuf_deps(self):
    pass

  def utf8_range_deps(self):
    pass

  def pip_parse(self, **kwargs):
    pass

  def rules_fuzzing_dependencies(self):
    pass

  def rules_fuzzing_init(self):
    pass

  def rules_pkg_dependencies(self):
    pass

  def system_python(self, **kwargs):
    pass

  def register_system_python(self, **kwargs):
    pass

  def register_toolchains(self, toolchain):
    pass

  def python_source_archive(self, **kwargs):
    pass

  def python_nuget_package(self, **kwargs):
    pass

  def install_deps(self):
    pass

  def fuzzing_py_install_deps(self):
    pass

  def googletest_deps(self):
    pass


class Converter(object):
  def __init__(self):
    self.prelude = ""
    self.toplevel = ""
    self.if_lua = ""
    self.utf8_range_commit = ""
    self.export_targets = []
    self.export_codegen_targets = []

  def convert(self):
    return self.template % {
        "prelude": converter.prelude,
        "toplevel": converter.toplevel,
        "utf8_range_commit": converter.utf8_range_commit,
        "export_targets": ' '.join(converter.export_targets),
        "export_codegen_targets": ' '.join(converter.export_codegen_targets),
    }

  template = textwrap.dedent("""\
    # This file was generated from BUILD using tools/make_cmakelists.py.

    cmake_minimum_required(VERSION 3.10...3.24)

    %(prelude)s

    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
    if(CMAKE_SOURCE_DIR STREQUAL upb_SOURCE_DIR)
      if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
        set(CMAKE_CXX_STANDARD 23)
      elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
        set(CMAKE_CXX_STANDARD 20)
      else()
        set(CMAKE_CXX_STANDARD 17)
      endif()
      set(CMAKE_CXX_STANDARD_REQUIRED ON)
    endif()

    # Prevent CMake from setting -rdynamic on Linux (!!).
    SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
    SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

    # Set default build type.
    if(NOT CMAKE_BUILD_TYPE)
      message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
      set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
          "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
          FORCE)
    endif()

    # When using Ninja, compiler output won't be colorized without this.
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG(-fdiagnostics-color=always SUPPORTS_COLOR_ALWAYS)
    if(SUPPORTS_COLOR_ALWAYS)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    endif()

    # Implement ASAN/UBSAN options
    if(UPB_ENABLE_ASAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    endif()

    if(UPB_ENABLE_UBSAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    endif()

    find_package(utf8_range QUIET)
    if(TARGET utf8_range::utf8_range)
      add_library(utf8_range ALIAS utf8_range::utf8_range)
      if(EXISTS "${utf8_range_DIR}/../../include/utf8_range.h")
        include_directories("${utf8_range_DIR}/../../include/")
      elseif(EXISTS "${utf8_range_DIR}/../../../include/utf8_range.h")
        include_directories("${utf8_range_DIR}/../../../include/")
      endif()
    elseif(EXISTS ../external/utf8_range)
      # utf8_range is already installed
      set(utf8_range_ENABLE_TESTS FALSE CACHE BOOL "")
      set(utf8_range_ENABLE_INSTALL TRUE CACHE BOOL "")
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/upb-utf8_range")
      add_subdirectory(../external/utf8_range "${CMAKE_CURRENT_BINARY_DIR}/upb-utf8_range")
      target_include_directories(utf8_range PUBLIC "\$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../external/utf8_range>")
    else()
      include(FetchContent)
      FetchContent_Declare(
        utf8_range
        GIT_REPOSITORY "https://github.com/protocolbuffers/utf8_range.git"
        GIT_TAG "%(utf8_range_commit)s"
      )
      FetchContent_GetProperties(utf8_range)
      if(NOT utf8_range_POPULATED)
        FetchContent_Populate(utf8_range)
        set(utf8_range_ENABLE_TESTS FALSE CACHE BOOL "")
        set(utf8_range_ENABLE_INSTALL TRUE CACHE BOOL "")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/upb-utf8_range")
        add_subdirectory("${utf8_range_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/upb-utf8_range")
        target_include_directories(utf8_range PUBLIC "\$<BUILD_INTERFACE:${utf8_range_SOURCE_DIR}>")
      endif()
    endif()

    if(APPLE)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup -flat_namespace")
    elseif(UNIX)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--build-id")
    endif()

    if (MSVC)
      add_compile_options(/wd4146 /wd4703 -D_CRT_SECURE_NO_WARNINGS)
    endif()

    enable_testing()

    if (UPB_ENABLE_CODEGEN)
      find_package(absl CONFIG REQUIRED)
      find_package(protobuf CONFIG REQUIRED)
      if(NOT UPB_HOST_INCLUDE_DIR)
        if(TARGET protobuf::libprotobuf)
          get_target_property(UPB_HOST_INCLUDE_DIR protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
        elseif(Protobuf_INCLUDE_DIR)
          set(UPB_HOST_INCLUDE_DIR "${Protobuf_INCLUDE_DIR}")
        else()
          set(UPB_HOST_INCLUDE_DIR "${PROTOBUF_INCLUDE_DIR}")
        endif()
      endif()
    endif()

    %(toplevel)s

    if (UPB_ENABLE_CODEGEN)
      set(UPB_CODEGEN_TARGETS protoc-gen-lua)
      add_executable(protoc-gen-lua
        ../lua/upbc.cc
      )
      target_link_libraries(protoc-gen-lua PRIVATE
        absl::strings
        protobuf::libprotobuf
        protobuf::libprotoc
      )

      set(PROTOC_PROGRAM "\$<TARGET_FILE:protobuf::protoc>")
      set(PROTOC_GEN_UPB_PROGRAM "\$<TARGET_FILE:protoc-gen-upb>")
      set(PROTOC_GEN_UPBDEFS_PROGRAM "\$<TARGET_FILE:protoc-gen-upbdefs>")
      set(PROTOC_GEN_UPBLUA_PROGRAM "\$<TARGET_FILE:protoc-gen-lua>")

      set(UPB_COMPILER_PLUGIN_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/google/protobuf/compiler/plugin.upb.h"
        "${CMAKE_CURRENT_BINARY_DIR}/google/protobuf/compiler/plugin.upb.c"
        "${CMAKE_CURRENT_BINARY_DIR}/google/protobuf/compiler/plugin.upbdefs.h"
        "${CMAKE_CURRENT_BINARY_DIR}/google/protobuf/compiler/plugin.upbdefs.c"
      )

      unset(UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_LUAS)
      unset(UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_HEADERS)
      unset(UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_SOURCES)
      unset(UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_PROTO_FILES)
      set(UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_PROTO_NAMES any api duration empty
          field_mask source_context struct timestamp type wrappers)
      foreach(PROTO_NAME IN LISTS UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_PROTO_NAMES)
        list(APPEND UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_PROTO_FILES
              "${UPB_HOST_INCLUDE_DIR}/google/protobuf/${PROTO_NAME}.proto")
        list(APPEND UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_LUAS
              "${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types/google/protobuf/${PROTO_NAME}_pb.lua")
        list(APPEND UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_HEADERS
              "${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types/google/protobuf/${PROTO_NAME}.upb.h"
              "${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types/google/protobuf/${PROTO_NAME}.upbdefs.h")
        list(APPEND UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_SOURCES
              "${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types/google/protobuf/${PROTO_NAME}.upb.c"
              "${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types/google/protobuf/${PROTO_NAME}.upbdefs.c")
      endforeach()

      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage2")
      add_custom_command(
        OUTPUT ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_LUAS}
              ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_HEADERS}
              ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_SOURCES}
        DEPENDS ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_PROTO_FILES}
        COMMAND
          "${PROTOC_PROGRAM}"
          "-I${UPB_HOST_INCLUDE_DIR}"
          "--plugin=protoc-gen-upb=${PROTOC_GEN_UPB_PROGRAM}"
          "--plugin=protoc-gen-upbdefs=${PROTOC_GEN_UPBDEFS_PROGRAM}"
          "--plugin=protoc-gen-lua=${PROTOC_GEN_UPBLUA_PROGRAM}"
          "--upb_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types"
          "--upbdefs_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types"
          "--lua_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types"
          ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_PROTO_FILES}
      )

      add_library(well_known_types ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_HEADERS}
        ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_SOURCES})
      target_include_directories(well_known_types PUBLIC "\$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/stage2/well_known_types>")
      set_target_properties(well_known_types PROPERTIES OUTPUT_NAME "upb-well_known_types")
      target_link_libraries(well_known_types PUBLIC upb descriptor_upb_proto)
    endif()

    include(GNUInstallDirs)
    install(
      DIRECTORY ../upb
      DESTINATION include
      FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.inc"
    )
    target_include_directories(upb INTERFACE $<INSTALL_INTERFACE:include>)
    install(TARGETS
      %(export_targets)s
      EXPORT upb-config
    )
    if (UPB_ENABLE_CODEGEN)
      install(
        FILES
          ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_LUAS}
          ${UPB_DESCRIPTOR_UPB_WELL_KNOWN_TYPES_HEADERS}
        DESTINATION include/google/protobuf
      )
      install(
        DIRECTORY ../lua/
        DESTINATION share/upb/lua
      )
      install(TARGETS
        well_known_types
        %(export_codegen_targets)s
        ${UPB_CODEGEN_TARGETS}
        EXPORT upb-config
      )
    endif()
    install(EXPORT upb-config NAMESPACE upb:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/upb")

  """)

data = {}
converter = Converter()

def GetDict(obj):
  ret = {}
  ret["UPB_DEFAULT_COPTS"] = []  # HACK
  ret["UPB_DEFAULT_CPPOPTS"] = []  # HACK
  for k in dir(obj):
    if not k.startswith("_"):
      ret[k] = getattr(obj, k);
  return ret

globs = GetDict(converter)

workspace_dict = GetDict(WorkspaceFileFunctions(converter))
exec(open("bazel/workspace_deps.bzl").read(), workspace_dict)
exec(open("WORKSPACE").read(), workspace_dict)
exec(open("BUILD").read(), GetDict(BuildFileFunctions(converter, "")))
exec(open("upb/util/BUILD").read(), GetDict(BuildFileFunctions(converter, "upb/util/")))
exec(open("upbc/BUILD").read(), GetDict(BuildFileFunctions(converter, "upbc/")))

with open(sys.argv[1], "w") as f:
  f.write(converter.convert())
