# This script is to be used to provide a function to
# call the protoc executable using the upb plugins.
#
# The following variables can be set and are optional:
#
# PROTOBUF_SRC_ROOT_FOLDER - When compiling with MSVC, if this cache variable is
#                            set the protobuf-default VS project build locations
#                            (vsprojects/Debug & vsprojects/Release) will be
#                            searched for libraries and binaries.
#
# PROTOC_PATH              - Path to protoc executable
# UPB_PLUGIN_PATH          - Path to protoc-gen-upb executable
# UPBDEF_PLUGIN_PATH       - Path to protoc-gen-upbdef executable
#
# UPB_IMPORT_DIRS          - List of additional directories to be searched for
#                            imported .proto files.
#
# UPB_GENERATE_X_APPEND_PATH    - By default -I will be passed to protoc for
#                                 each directory where a proto file is referenced.
#                                 This causes all output files to go directly under
#                                 build directory, instead of mirroring relative
#                                 paths of source directories. Set to FALSE
#                                 if you want to disable this behaviour.
#
# Defines the following variables:
#
# The following cache variables are also available to set or use:
# PROTOBUF_PROTOC_EXECUTABLE - The protoc compiler (full path if not found in ${PATH})
# UPB_GENERATOR     - The upb plugin executable (full path if not found in ${PATH})
# UPBDEFS_GENERATOR - The upbdefs plugin executable (full path if not found in ${PATH})
#
# =============================================================================
# Copyright 2009 Kitware, Inc. Copyright 2009-2011 Philip Lowman
# <philip@yhbt.com> Copyright 2008 Esben Mose Hansen, Ange Optimization ApS
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the names of Kitware, Inc., the Insight Software Consortium, nor the
#   names of their contributors may be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# =============================================================================
#
# Changes 2021.08.12 - Max Christy - used Modules/FindProtobuf.cmake from cmake
# 2.8.10 to write ProtocGenUpb.cmake
#
# =============================================================================

# compile a list of possible alternate locations for executables so we can make
# a good guess at where they are if not found in the PATH environment variable
set(_HINT_PATHS)
list(
  APPEND
  _HINT_PATHS
  "${PROTOC_PATH}"
  "${UPB_PLUGIN_PATH}"
  "${UPBDEF_PLUGIN_PATH}"
  "${PROTOBUF_SRC_ROOT_FOLDER}/bin"
)

foreach(config ${CMAKE_CONFIGURATION_TYPES})
  list(
    APPEND
    _HINT_PATHS
    ${CMAKE_BINARY_DIR}/${config}
    ${PROTOBUF_SRC_ROOT_FOLDER}/vsprojects/${config}
  )
endforeach()

# Find the protoc Executable
if(NOT PROTOBUF_PROTOC_EXECUTABLE)
  find_program(
    PROTOBUF_PROTOC_EXECUTABLE
    NAMES protoc
    DOC "The Google Protocol Buffers Compiler"
    HINTS ${_HINTS}
  )
  mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
  if(PROTOBUF_PROTOC_EXECUTABLE)
    message(STATUS "Library upb executable protoc found: ${PROTOBUF_PROTOC_EXECUTABLE}")
  else()
    message(STATUS "Library upb executable protoc NOT found!")
  endif()
else()
    message(STATUS "Library upb executable protoc found: ${PROTOBUF_PROTOC_EXECUTABLE}")
endif()

# Find the protoc-gen-upb Executable
if(NOT UPB_GENERATOR)
  find_program(
    UPB_GENERATOR
    NAMES protoc-gen-upb
    DOC "protoc-gen-upb pluggin to protoc for the generation of μpb (often written 'upb') code"
    HINTS ${_HINT_PATHS}
  )
  if(UPB_GENERATOR)
    message(STATUS "Library upb executable protoc-gen-upb found: ${UPB_GENERATOR}")
  else()
    message(STATUS "Library upb executable protoc-gen-upb NOT found!")
  endif()
else()
    message(STATUS "Library upb executable protoc-gen-upb found: ${UPB_GENERATOR}")
endif()
get_filename_component(UPB_GENERATOR_NAME ${UPB_GENERATOR} NAME_WE)
get_filename_component(UPB_GENERATOR_DIR ${UPB_GENERATOR} DIRECTORY)

# Find the protoc-gen-upbdefs Executable
if(NOT UPBDEFS_GENERATOR)
  find_program(
    UPBDEFS_GENERATOR
    NAMES protoc-gen-upbdefs
    DOC "protoc-gen-upbdefs pluggin to protoc for the generation of μpb (often written 'upb') reflection code"
    HINTS ${_HINT_PATHS}
  if(UPBDEFS_GENERATOR)
    message(STATUS "Library upb executable protoc-gen-upbdefs found: ${UPBDEFS_GENERATOR}")
  else()
    message(STATUS "Library upb executable protoc-gen-upbdefs NOT found!")
  endif()
else()
    message(STATUS "Library upb executable protoc-gen-upbupb found: ${UPBDEFS_GENERATOR}")
  )
endif()
get_filename_component(UPBDEFS_GENERATOR_NAME ${UPBDEFS_GENERATOR} NAME_WE)
get_filename_component(UPBDEFS_GENERATOR_DIR ${UPBDEFS_GENERATOR} DIRECTORY)
unset(_HINT_PATHS)

# ====================================================================
#
# UPB_GENERATE_C (public function)
# UPB_GENERATE_C(SRCS HDRS [RELPATH <root-path-of-proto-files>] <proto-files>...)
#   SRCS = Variable to define with autogenerated source files
#   HDRS = Variable to define with autogenerated header files
#   RELPATH (optional): Use if you want to use relative paths in your import statements
#     The argument to RELPATH should be the directory that all the
#     imports will be relative to. When RELPATH is not specified then all proto
#     files can be imported without a path.
#
# ====================================================================
# Example:
#
# find_package( upb REQUIRED )
# upb_generate_c(PROTO_SRCS PROTO_HDRS foo.proto)
#
# add_executable(bar)
# target_sources(bar PRIVATE bar.c ${PROTO_SRCS} ${PROTO_HDRS})
# target_include_directories(bar PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
# target_link_libraries(bar PUBLIC upb::upb)
#
#
# Example with RELPATH:
# Assume we have a layout like:
#  .../CMakeLists.txt
#  .../bar.cc
#  .../proto/
#  .../proto/foo.proto (Which contains: import "sub/bar.proto"; )
#  .../proto/sub/bar.proto
#
# Everything would be the same as the previous example, but the call to
# upb_generate_c would change to:
#
# upb_generate_c(PROTO_SRCS PROTO_HDRS RELPATH proto proto/foo.proto
# proto/sub/bar.proto)
#
# ====================================================================

function(UPB_GENERATE_C SRCS HDRS)
  # cmake-format: off
  cmake_parse_arguments(UPB_GENERATE_C "" "RELPATH" "" ${ARGN})
  if(NOT UPB_GENERATE_C_UNPARSED_ARGUMENTS)
    message(SEND_ERROR "Error: UPB_GENERATE_C() called without any proto files")
    return()
  endif()

  # Construct include list
  set(_upb_include_set)
  foreach(_dir ${UPB_INCLUDE_DIRS})
    list(APPEND _upb_include_set -I${_dir})
  endforeach()

  if(UPB_GENERATE_X_APPEND_PATH)
    # Create an include path for each file specified
    foreach(FIL ${UPB_GENERATE_C_UNPARSED_ARGUMENTS})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(ABS_PATH ${ABS_FIL} PATH)
      list(FIND _upb_include_set ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _upb_include_set -I ${ABS_PATH})
      endif()
    endforeach()
  else()
    set(_upb_include_set -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(DEFINED UPB_IMPORT_DIRS)
    foreach(DIR ${UPB_IMPORT_DIRS})
      get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
      list(FIND _upb_include_set ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _upb_include_set -I ${ABS_PATH})
      endif()
    endforeach()
  endif()
  if(UPB_GENERATE_C_RELPATH)
    list(APPEND _upb_include_set "-I${UPB_GENERATE_C_RELPATH}")
  endif()
  list(REMOVE_DUPLICATES _upb_include_set)
  # cmake-format: on

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    get_filename_component(ABS_DIR ${ABS_FIL} DIRECTORY)

    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upb.c")
    list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upb.h")

    # cmake-format: off
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upb.c"
             "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upb.h"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS
        --plugin=${UPB_GENERATOR_NAME}=${UPB_GENERATOR}
        --proto_path ${ABS_DIR}
        --upb_out ${CMAKE_CURRENT_BINARY_DIR} 
        ${_upb_include_set}
        ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE} ${UPB_GENERATOR}
      COMMENT "Executing ${UPB_GENERATOR_NAME} on ${FIL}" VERBATIM)
    # cmake-format: on
    unset(ABS_FIL)
    unset(FIL_WE)
    unset(ABS_DIR)
  endforeach()
  unset(_upb_include_set)

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS}
      ${${SRCS}}
      PARENT_SCOPE
  )
  set(${HDRS}
      ${${HDRS}}
      PARENT_SCOPE
  )
endfunction()

# ====================================================================
#
# UPB_GENERATE_DEFS (public function)
# UPB_GENERATE_DEFS(SRCS HDRS [RELPATH <root-path-of-proto-files>] <proto-files>...)
#   SRCS = Variable to define with autogenerated source files
#   HDRS = Variable to define with autogenerated header files
#   RELPATH (optional): Use if you want to use relative paths in your import statements
#     The argument to RELPATH should be the directory that all the
#     imports will be relative to. When RELPATH is not specified then all proto
#     files can be imported without a path.
#
# ====================================================================
# Example:
#
# find_package( upb REQUIRED )
# upb_generate_defs(PROTO_SRCS PROTO_HDRS foo.proto)
#
# add_executable(bar)
# target_sources(bar PRIVATE bar.c ${PROTO_SRCS} ${PROTO_HDRS})
# target_include_directories(bar PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
# target_link_libraries(bar PUBLIC upb::upb)
#
#
# Example with RELPATH:
# Assume we have a layout like:
#  ../CMakeLists.txt
#  ../bar.cc
#  ../proto/
#  ../proto/foo.proto (Which contains: import "sub/bar.proto"; )
#  ../proto/sub/bar.proto
#
# Everything would be the same as the previous example, but the call to
# upb_generate_c would change to:
#
# upb_generate_c(PROTO_SRCS PROTO_HDRS RELPATH proto proto/foo.proto
# proto/sub/bar.proto)
#
# ===================================================================
function(UPB_GENERATE_DEFS SRCS HDRS)
  # cmake-format: off
  cmake_parse_arguments(UPB_GENERATE_DEFS "" "RELPATH" "" ${ARGN})
  if(NOT UPB_GENERATE_DEFS_UNPARSED_ARGUMENTS)
    message(SEND_ERROR "Error: UPB_GENERATE_DEFS() called without any proto files")
    return()
  endif()

  # Construct include list
  set(_upb_include_set)
  foreach(_dir ${UPB_INCLUDE_DIRS})
    list(APPEND _upb_include_set -I${_dir})
  endforeach()

  if(UPB_GENERATE_X_APPEND_PATH)
    # Create an include path for each file specified
    foreach(FIL ${UPB_GENERATE_C_UNPARSED_ARGUMENTS})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(ABS_PATH ${ABS_FIL} PATH)
      list(FIND _upb_include_set ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _upb_include_set -I ${ABS_PATH})
      endif()
    endforeach()
  else()
    set(_upb_include_set -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(DEFINED UPB_IMPORT_DIRS)
    foreach(DIR ${UPB_IMPORT_DIRS})
      get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
      list(FIND _upb_include_set ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _upb_include_set -I ${ABS_PATH})
      endif()
    endforeach()
  endif()
  if(UPB_GENERATE_DEFS_RELPATH)
    list(APPEND _upb_include_set "-I${UPB_GENERATE_DEFS_RELPATH}")
  endif()
  list(REMOVE_DUPLICATES _upb_include_set)
  # cmake-format: on

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    get_filename_component(ABS_DIR ${ABS_FIL} DIRECTORY)

    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upbdefs.c")
    list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upbdefs.h")

    # cmake-format: off
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upbdefs.c"
             "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.upbdefs.h"
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS
        --plugin=${UPBDEFS_GENERATOR_NAME}=${UPBDEFS_GENERATOR}
        --proto_path ${ABS_DIR} 
        --upbdefs_out ${CMAKE_CURRENT_BINARY_DIR} 
        ${_upb_include_set}
        ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE} ${UPB_GENERATOR} ${UPBDEFS_GENERATOR}
      COMMENT "Executing ${UPBDEFS_GENERATOR_NAME} on ${FIL}" VERBATIM)
    # cmake-format: on
    unset(ABS_FIL)
    unset(FIL_WE)
    unset(ABS_DIR)
  endforeach()
  unset(_upb_include_set)

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS}
      ${${SRCS}}
      PARENT_SCOPE
  )
  set(${HDRS}
      ${${HDRS}}
      PARENT_SCOPE
  )
endfunction()
