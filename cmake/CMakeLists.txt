# This file was generated from BUILD using tools/make_cmakelists.py.

cmake_minimum_required(VERSION 3.10...3.24)

project(upb)
set(CMAKE_C_STANDARD 99)


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
    GIT_TAG "de0b4a8ff9b5d4c98108bdfe723291a33c52c54f"
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


add_library(port INTERFACE
    
)
target_include_directories(port INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(port INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()

add_library(upb INTERFACE
    
)
target_include_directories(upb INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(upb INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(upb INTERFACE
  base
  collections_internal
  fastdecode
  hash
  lex
  mem
  message_internal
  mini_table_internal
  port
  wire)

add_library(base 
    ../upb/base/status.c
  ../upb/base/descriptor_constants.h
  ../upb/base/log2.h
  ../upb/base/status.h
  ../upb/base/string_view.h
)
target_include_directories(base PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(base PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(base PUBLIC
  port)

add_library(mini_table INTERFACE
    
)
target_include_directories(mini_table INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(mini_table INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(mini_table INTERFACE
  base
  mem
  mini_table_internal
  port)

add_library(mini_table_internal 
    ../upb/mini_table/common.c
  ../upb/mini_table/decode.c
  ../upb/mini_table/encode.c
  ../upb/mini_table/extension_registry.c
  ../upb/mini_table/message_internal.c
  ../upb/mini_table/common.h
  ../upb/mini_table/common_internal.h
  ../upb/mini_table/decode.h
  ../upb/mini_table/encode_internal.h
  ../upb/mini_table/encode_internal.hpp
  ../upb/mini_table/enum_internal.h
  ../upb/mini_table/extension_internal.h
  ../upb/mini_table/extension_registry.h
  ../upb/mini_table/field_internal.h
  ../upb/mini_table/file_internal.h
  ../upb/mini_table/message_internal.h
  ../upb/mini_table/sub_internal.h
  ../upb/mini_table/types.h
)
target_include_directories(mini_table_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(mini_table_internal PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(mini_table_internal PUBLIC
  base
  hash
  mem
  port)

add_library(message INTERFACE
    
)
target_include_directories(message INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message INTERFACE
  mem
  message_internal
  mini_table
  port)

add_library(message_internal 
    ../upb/message/message.c
  ../upb/message/extension_internal.h
  ../upb/message/internal.h
  ../upb/message/message.h
)
target_include_directories(message_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message_internal PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message_internal PUBLIC
  base
  hash
  mem
  mini_table_internal
  port)

add_library(message_accessors_internal INTERFACE
    
)
target_include_directories(message_accessors_internal INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message_accessors_internal INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message_accessors_internal INTERFACE
  collections_internal
  message_internal
  mini_table_internal
  port)

add_library(message_accessors 
    ../upb/message/accessors.c
  ../upb/message/accessors_internal.h
  ../upb/message/accessors.h
)
target_include_directories(message_accessors PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message_accessors PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message_accessors PUBLIC
  collections_internal
  eps_copy_input_stream
  hash
  message_internal
  mini_table_internal
  port
  upb
  wire
  wire_reader)

add_library(message_promote 
    ../upb/message/promote.c
  ../upb/message/promote.h
)
target_include_directories(message_promote PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message_promote PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message_promote PUBLIC
  collections_internal
  eps_copy_input_stream
  hash
  message_accessors
  message_internal
  mini_table_internal
  port
  upb
  wire
  wire_reader)

add_library(message_copy 
    ../upb/message/copy.c
  ../upb/message/copy.h
)
target_include_directories(message_copy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message_copy PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message_copy PUBLIC
  collections_internal
  message_accessors
  message_internal
  mini_table_internal
  port
  upb)

add_library(message_split64 INTERFACE
    
)
target_include_directories(message_split64 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(message_split64 INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(message_split64 INTERFACE
  message_accessors
  port)

add_library(fastdecode INTERFACE
    
)
target_include_directories(fastdecode INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(fastdecode INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(fastdecode INTERFACE
  base
  collections_internal
  hash
  mem_internal
  message_internal
  mini_table_internal
  port
  wire)

add_library(generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
    
)
target_include_directories(generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
  base
  collections_internal
  hash
  upb)

add_library(generated_cpp_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
    
)
target_include_directories(generated_cpp_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(generated_cpp_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(generated_cpp_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
  base
  collections_internal
  hash
  message_copy
  mini_table
  upb)

add_library(generated_reflection_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
    
)
target_include_directories(generated_reflection_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(generated_reflection_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(generated_reflection_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me INTERFACE
  base
  descriptor_upb_proto
  hash
  mini_table_internal
  reflection_internal)
if (UPB_ENABLE_CODEGEN)

add_library(descriptor_upb_proto_stage0 
    ../upb/reflection/stage0/google/protobuf/descriptor.upb.h
  ../upb/reflection/stage0/google/protobuf/descriptor.upb.c
)
target_include_directories(descriptor_upb_proto_stage0 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(descriptor_upb_proto_stage0 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(descriptor_upb_proto_stage0
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../upb/reflection/stage0>")
target_link_libraries(descriptor_upb_proto_stage0 PUBLIC
  generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me
  mini_table)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto")
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto/google/protobuf/descriptor.upb.h
    ${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto/google/protobuf/descriptor.upb.c
  DEPENDS
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/descriptor.proto
  COMMAND
    "${PROTOC_PROGRAM}"
    "-I${UPB_HOST_INCLUDE_DIR}"
    "--plugin=protoc-gen-upb=\$<TARGET_FILE:protoc-gen-upb_stage0>"
    "--upb_out=${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto"
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/descriptor.proto
)

add_library(descriptor_upb_proto_stage1 
    ${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto/google/protobuf/descriptor.upb.h
  ${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto/google/protobuf/descriptor.upb.c
)
target_include_directories(descriptor_upb_proto_stage1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(descriptor_upb_proto_stage1 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(descriptor_upb_proto_stage1
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_stage1/descriptor_upb_proto>")
target_link_libraries(descriptor_upb_proto_stage1 PUBLIC
  generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto")
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upb.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upb.c
  DEPENDS
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/descriptor.proto
  COMMAND
    "${PROTOC_PROGRAM}"
    "-I${UPB_HOST_INCLUDE_DIR}"
    "--plugin=protoc-gen-upb=\$<TARGET_FILE:protoc-gen-upb_stage1>"
    "--upb_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto"
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/descriptor.proto
)
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upbdefs.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upbdefs.c
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor_pb.lua
  DEPENDS
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/descriptor.proto
  COMMAND
    "${PROTOC_PROGRAM}"
    "-I${UPB_HOST_INCLUDE_DIR}"
    "--plugin=protoc-gen-upbdefs=\$<TARGET_FILE:protoc-gen-upbdefs>"
    "--plugin=protoc-gen-lua=\$<TARGET_FILE:protoc-gen-lua>"
    "--upbdefs_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto"
    "--lua_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto"
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/descriptor.proto
)

add_library(descriptor_upb_proto 
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upb.h
  ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upb.c
)
target_include_directories(descriptor_upb_proto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(descriptor_upb_proto PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(descriptor_upb_proto
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto>")
target_link_libraries(descriptor_upb_proto PUBLIC
  upb
)

add_library(descriptor_upb_proto_defs 
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upbdefs.h
  ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upbdefs.c
)
target_include_directories(descriptor_upb_proto_defs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(descriptor_upb_proto_defs PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(descriptor_upb_proto_defs
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto>")
target_link_libraries(descriptor_upb_proto_defs PUBLIC
  descriptor_upb_proto
)
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upb.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upb.c
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upbdefs.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor.upbdefs.c
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/descriptor_upb_proto/google/protobuf/descriptor_pb.lua
  DESTINATION "include/google/protobuf"
)
endif()

add_library(collections INTERFACE
    
)
target_include_directories(collections INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(collections INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(collections INTERFACE
  base
  collections_internal
  mem
  port)

add_library(collections_internal 
    ../upb/collections/array.c
  ../upb/collections/map.c
  ../upb/collections/map_sorter.c
  ../upb/collections/array.h
  ../upb/collections/array_internal.h
  ../upb/collections/map.h
  ../upb/collections/map_gencode_util.h
  ../upb/collections/map_internal.h
  ../upb/collections/map_sorter_internal.h
  ../upb/collections/message_value.h
)
target_include_directories(collections_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(collections_internal PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(collections_internal PUBLIC
  base
  hash
  mem
  message_internal
  mini_table_internal
  port)

add_library(collections_split64 INTERFACE
    
)
target_include_directories(collections_split64 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(collections_split64 INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(collections_split64 INTERFACE
  collections
  port)
if (UPB_ENABLE_CODEGEN)

add_library(reflection_stage0 INTERFACE
    
)
target_include_directories(reflection_stage0 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(reflection_stage0 INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(reflection_stage0 INTERFACE
  collections
  port
  upb)
target_link_libraries(reflection_stage0 INTERFACE
  reflection_internal_stage0)

add_library(reflection_stage1 INTERFACE
    
)
target_include_directories(reflection_stage1 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(reflection_stage1 INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(reflection_stage1 INTERFACE
  collections
  port
  upb)
target_link_libraries(reflection_stage1 INTERFACE
  reflection_internal_stage1)

add_library(reflection INTERFACE
    
)
target_include_directories(reflection INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(reflection INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(reflection INTERFACE
  collections
  port
  upb)
target_link_libraries(reflection INTERFACE
  reflection_internal)
endif()
if (UPB_ENABLE_CODEGEN)

add_library(reflection_internal_stage0 
    ../upb/reflection/def_builder.c
  ../upb/reflection/def_pool.c
  ../upb/reflection/def_type.c
  ../upb/reflection/desc_state.c
  ../upb/reflection/enum_def.c
  ../upb/reflection/enum_reserved_range.c
  ../upb/reflection/enum_value_def.c
  ../upb/reflection/extension_range.c
  ../upb/reflection/field_def.c
  ../upb/reflection/file_def.c
  ../upb/reflection/message.c
  ../upb/reflection/message_def.c
  ../upb/reflection/message_reserved_range.c
  ../upb/reflection/method_def.c
  ../upb/reflection/oneof_def.c
  ../upb/reflection/service_def.c
  ../upb/reflection/common.h
  ../upb/reflection/def.h
  ../upb/reflection/def.hpp
  ../upb/reflection/def_builder_internal.h
  ../upb/reflection/def_pool.h
  ../upb/reflection/def_pool_internal.h
  ../upb/reflection/def_type.h
  ../upb/reflection/desc_state_internal.h
  ../upb/reflection/enum_def.h
  ../upb/reflection/enum_def_internal.h
  ../upb/reflection/enum_reserved_range.h
  ../upb/reflection/enum_reserved_range_internal.h
  ../upb/reflection/enum_value_def.h
  ../upb/reflection/enum_value_def_internal.h
  ../upb/reflection/extension_range.h
  ../upb/reflection/extension_range_internal.h
  ../upb/reflection/field_def.h
  ../upb/reflection/field_def_internal.h
  ../upb/reflection/file_def.h
  ../upb/reflection/file_def_internal.h
  ../upb/reflection/message.h
  ../upb/reflection/message.hpp
  ../upb/reflection/message_def.h
  ../upb/reflection/message_def_internal.h
  ../upb/reflection/message_reserved_range.h
  ../upb/reflection/message_reserved_range_internal.h
  ../upb/reflection/method_def.h
  ../upb/reflection/method_def_internal.h
  ../upb/reflection/oneof_def.h
  ../upb/reflection/oneof_def_internal.h
  ../upb/reflection/service_def.h
  ../upb/reflection/service_def_internal.h
)
target_include_directories(reflection_internal_stage0 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(reflection_internal_stage0 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(reflection_internal_stage0 PUBLIC
  collections
  hash
  message_accessors
  mini_table_internal
  port
  upb)
target_link_libraries(reflection_internal_stage0 PUBLIC
  descriptor_upb_proto_stage0)

add_library(reflection_internal_stage1 
    ../upb/reflection/def_builder.c
  ../upb/reflection/def_pool.c
  ../upb/reflection/def_type.c
  ../upb/reflection/desc_state.c
  ../upb/reflection/enum_def.c
  ../upb/reflection/enum_reserved_range.c
  ../upb/reflection/enum_value_def.c
  ../upb/reflection/extension_range.c
  ../upb/reflection/field_def.c
  ../upb/reflection/file_def.c
  ../upb/reflection/message.c
  ../upb/reflection/message_def.c
  ../upb/reflection/message_reserved_range.c
  ../upb/reflection/method_def.c
  ../upb/reflection/oneof_def.c
  ../upb/reflection/service_def.c
  ../upb/reflection/common.h
  ../upb/reflection/def.h
  ../upb/reflection/def.hpp
  ../upb/reflection/def_builder_internal.h
  ../upb/reflection/def_pool.h
  ../upb/reflection/def_pool_internal.h
  ../upb/reflection/def_type.h
  ../upb/reflection/desc_state_internal.h
  ../upb/reflection/enum_def.h
  ../upb/reflection/enum_def_internal.h
  ../upb/reflection/enum_reserved_range.h
  ../upb/reflection/enum_reserved_range_internal.h
  ../upb/reflection/enum_value_def.h
  ../upb/reflection/enum_value_def_internal.h
  ../upb/reflection/extension_range.h
  ../upb/reflection/extension_range_internal.h
  ../upb/reflection/field_def.h
  ../upb/reflection/field_def_internal.h
  ../upb/reflection/file_def.h
  ../upb/reflection/file_def_internal.h
  ../upb/reflection/message.h
  ../upb/reflection/message.hpp
  ../upb/reflection/message_def.h
  ../upb/reflection/message_def_internal.h
  ../upb/reflection/message_reserved_range.h
  ../upb/reflection/message_reserved_range_internal.h
  ../upb/reflection/method_def.h
  ../upb/reflection/method_def_internal.h
  ../upb/reflection/oneof_def.h
  ../upb/reflection/oneof_def_internal.h
  ../upb/reflection/service_def.h
  ../upb/reflection/service_def_internal.h
)
target_include_directories(reflection_internal_stage1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(reflection_internal_stage1 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(reflection_internal_stage1 PUBLIC
  collections
  hash
  message_accessors
  mini_table_internal
  port
  upb)
target_link_libraries(reflection_internal_stage1 PUBLIC
  descriptor_upb_proto_stage1)

add_library(reflection_internal 
    ../upb/reflection/def_builder.c
  ../upb/reflection/def_pool.c
  ../upb/reflection/def_type.c
  ../upb/reflection/desc_state.c
  ../upb/reflection/enum_def.c
  ../upb/reflection/enum_reserved_range.c
  ../upb/reflection/enum_value_def.c
  ../upb/reflection/extension_range.c
  ../upb/reflection/field_def.c
  ../upb/reflection/file_def.c
  ../upb/reflection/message.c
  ../upb/reflection/message_def.c
  ../upb/reflection/message_reserved_range.c
  ../upb/reflection/method_def.c
  ../upb/reflection/oneof_def.c
  ../upb/reflection/service_def.c
  ../upb/reflection/common.h
  ../upb/reflection/def.h
  ../upb/reflection/def.hpp
  ../upb/reflection/def_builder_internal.h
  ../upb/reflection/def_pool.h
  ../upb/reflection/def_pool_internal.h
  ../upb/reflection/def_type.h
  ../upb/reflection/desc_state_internal.h
  ../upb/reflection/enum_def.h
  ../upb/reflection/enum_def_internal.h
  ../upb/reflection/enum_reserved_range.h
  ../upb/reflection/enum_reserved_range_internal.h
  ../upb/reflection/enum_value_def.h
  ../upb/reflection/enum_value_def_internal.h
  ../upb/reflection/extension_range.h
  ../upb/reflection/extension_range_internal.h
  ../upb/reflection/field_def.h
  ../upb/reflection/field_def_internal.h
  ../upb/reflection/file_def.h
  ../upb/reflection/file_def_internal.h
  ../upb/reflection/message.h
  ../upb/reflection/message.hpp
  ../upb/reflection/message_def.h
  ../upb/reflection/message_def_internal.h
  ../upb/reflection/message_reserved_range.h
  ../upb/reflection/message_reserved_range_internal.h
  ../upb/reflection/method_def.h
  ../upb/reflection/method_def_internal.h
  ../upb/reflection/oneof_def.h
  ../upb/reflection/oneof_def_internal.h
  ../upb/reflection/service_def.h
  ../upb/reflection/service_def_internal.h
)
target_include_directories(reflection_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(reflection_internal PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(reflection_internal PUBLIC
  collections
  hash
  message_accessors
  mini_table_internal
  port
  upb)
target_link_libraries(reflection_internal PUBLIC
  descriptor_upb_proto)
endif()

add_library(textformat 
    ../upb/text/encode.c
  ../upb/text/encode.h
  ../upb/text_encode.h
)
target_include_directories(textformat PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(textformat PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(textformat PUBLIC
  collections_internal
  eps_copy_input_stream
  lex
  port
  reflection
  wire
  wire_reader
  wire_types)

add_library(json 
    ../upb/json/decode.c
  ../upb/json/encode.c
  ../upb/json/decode.h
  ../upb/json/encode.h
  ../upb/json_decode.h
  ../upb/json_encode.h
)
target_include_directories(json PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(json PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(json PUBLIC
  collections
  lex
  port
  reflection
  upb)

add_library(mem INTERFACE
    
)
target_include_directories(mem INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(mem INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(mem INTERFACE
  mem_internal
  port)

add_library(mem_internal 
    ../upb/mem/alloc.c
  ../upb/mem/arena.c
  ../upb/mem/alloc.h
  ../upb/mem/arena.h
  ../upb/mem/arena_internal.h
)
target_include_directories(mem_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(mem_internal PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(mem_internal PUBLIC
  port)

add_library(wire INTERFACE
    
)
target_include_directories(wire INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(wire INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(wire INTERFACE
  mem
  message_internal
  mini_table_internal
  port
  wire_internal)

add_library(wire_internal 
    ../upb/wire/decode.c
  ../upb/wire/decode_fast.c
  ../upb/wire/encode.c
  ../upb/wire/common.h
  ../upb/wire/common_internal.h
  ../upb/wire/decode.h
  ../upb/wire/decode_fast.h
  ../upb/wire/decode_internal.h
  ../upb/wire/encode.h
  ../upb/wire/swap_internal.h
)
target_include_directories(wire_internal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(wire_internal PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(wire_internal PUBLIC
  base
  collections_internal
  eps_copy_input_stream
  hash
  mem_internal
  message_accessors_internal
  message_internal
  mini_table_internal
  port
  wire_reader
  wire_types
  utf8_range)

add_library(wire_types INTERFACE
    
)
target_include_directories(wire_types INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(wire_types INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()

add_library(eps_copy_input_stream 
    ../upb/wire/eps_copy_input_stream.c
  ../upb/wire/eps_copy_input_stream.h
)
target_include_directories(eps_copy_input_stream PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(eps_copy_input_stream PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(eps_copy_input_stream PUBLIC
  mem
  port)

add_library(wire_reader 
    ../upb/wire/reader.c
  ../upb/wire/swap_internal.h
  ../upb/wire/reader.h
)
target_include_directories(wire_reader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(wire_reader PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(wire_reader PUBLIC
  eps_copy_input_stream
  port
  wire_types)

add_library(hash 
    ../upb/hash/common.c
  ../upb/hash/common.h
  ../upb/hash/int_table.h
  ../upb/hash/str_table.h
)
target_include_directories(hash PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(hash PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(hash PUBLIC
  base
  mem
  port)

add_library(lex 
    ../upb/lex/atoi.c
  ../upb/lex/round_trip.c
  ../upb/lex/strtod.c
  ../upb/lex/unicode.c
  ../upb/lex/atoi.h
  ../upb/lex/round_trip.h
  ../upb/lex/strtod.h
  ../upb/lex/unicode.h
)
target_include_directories(lex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(lex PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(lex PUBLIC
  port)

add_library(def_to_proto 
    ../upb/util/def_to_proto.c
  ../upb/util/def_to_proto.h
)
target_include_directories(def_to_proto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(def_to_proto PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(def_to_proto PUBLIC
  port
  reflection
  reflection_internal)

add_library(required_fields 
    ../upb/util/required_fields.c
  ../upb/util/required_fields.h
)
target_include_directories(required_fields PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(required_fields PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(required_fields PUBLIC
  collections
  port
  reflection)

add_library(compare 
    ../upb/util/compare.c
  ../upb/util/compare.h
)
target_include_directories(compare PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(compare PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(compare PUBLIC
  eps_copy_input_stream
  port
  upb
  wire_reader
  wire_types)
if (UPB_ENABLE_CODEGEN)

add_library(plugin_upb_proto_stage0 
    ../upbc/stage0/google/protobuf/compiler/plugin.upb.h
  ../upbc/stage0/google/protobuf/compiler/plugin.upb.c
)
target_include_directories(plugin_upb_proto_stage0 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin_upb_proto_stage0 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(plugin_upb_proto_stage0
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../upbc/stage0>")
target_link_libraries(plugin_upb_proto_stage0 PUBLIC
  generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me
  mini_table)
target_link_libraries(plugin_upb_proto_stage0 PUBLIC
  descriptor_upb_proto_stage0)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto")
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto/google/protobuf/compiler/plugin.upb.h
    ${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto/google/protobuf/compiler/plugin.upb.c
  DEPENDS
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto
  COMMAND
    "${PROTOC_PROGRAM}"
    "-I${UPB_HOST_INCLUDE_DIR}"
    "--plugin=protoc-gen-upb=\$<TARGET_FILE:protoc-gen-upb_stage0>"
    "--upb_out=${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto"
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto
)

add_library(plugin_upb_proto_stage1 
    ${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto/google/protobuf/compiler/plugin.upb.h
  ${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto/google/protobuf/compiler/plugin.upb.c
)
target_include_directories(plugin_upb_proto_stage1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin_upb_proto_stage1 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(plugin_upb_proto_stage1
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_stage1/plugin_upb_proto>")
target_link_libraries(plugin_upb_proto_stage1 PUBLIC
  generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me
)
target_link_libraries(plugin_upb_proto_stage1 PUBLIC
  descriptor_upb_proto_stage1)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto")
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upb.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upb.c
  DEPENDS
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto
  COMMAND
    "${PROTOC_PROGRAM}"
    "-I${UPB_HOST_INCLUDE_DIR}"
    "--plugin=protoc-gen-upb=\$<TARGET_FILE:protoc-gen-upb_stage1>"
    "--upb_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto"
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto
)
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upbdefs.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upbdefs.c
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin_pb.lua
  DEPENDS
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto
  COMMAND
    "${PROTOC_PROGRAM}"
    "-I${UPB_HOST_INCLUDE_DIR}"
    "--plugin=protoc-gen-upbdefs=\$<TARGET_FILE:protoc-gen-upbdefs>"
    "--plugin=protoc-gen-lua=\$<TARGET_FILE:protoc-gen-lua>"
    "--upbdefs_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto"
    "--lua_out=${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto"
    ${UPB_HOST_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto
)

add_library(plugin_upb_proto 
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upb.h
  ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upb.c
)
target_include_directories(plugin_upb_proto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin_upb_proto PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(plugin_upb_proto
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto>")
target_link_libraries(plugin_upb_proto PUBLIC
  upb
)
target_link_libraries(plugin_upb_proto PUBLIC
  descriptor_upb_proto)

add_library(plugin_upb_proto_defs 
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upbdefs.h
  ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upbdefs.c
)
target_include_directories(plugin_upb_proto_defs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin_upb_proto_defs PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_include_directories(plugin_upb_proto_defs
  BEFORE PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto>")
target_link_libraries(plugin_upb_proto_defs PUBLIC
  plugin_upb_proto
)
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upb.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upb.c
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upbdefs.h
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin.upbdefs.c
    ${CMAKE_CURRENT_BINARY_DIR}/stage2/plugin_upb_proto/google/protobuf/compiler/plugin_pb.lua
  DESTINATION "include/google/protobuf/compiler"
)
endif()
if (UPB_ENABLE_CODEGEN)

add_library(common_stage0 
    ../upbc/common.cc
  ../upbc/common.h
)
target_include_directories(common_stage0 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(common_stage0 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(common_stage0 PUBLIC
  absl::strings)
target_link_libraries(common_stage0 PUBLIC
  reflection_stage0)

add_library(common_stage1 
    ../upbc/common.cc
  ../upbc/common.h
)
target_include_directories(common_stage1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(common_stage1 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(common_stage1 PUBLIC
  absl::strings)
target_link_libraries(common_stage1 PUBLIC
  reflection_stage1)

add_library(common 
    ../upbc/common.cc
  ../upbc/common.h
)
target_include_directories(common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(common PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(common PUBLIC
  absl::strings)
target_link_libraries(common PUBLIC
  reflection)
endif()
if (UPB_ENABLE_CODEGEN)

add_library(file_layout_stage0 
    ../upbc/file_layout.cc
  ../upbc/file_layout.h
)
target_include_directories(file_layout_stage0 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(file_layout_stage0 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(file_layout_stage0 PUBLIC
  mini_table
  mini_table_internal
  port
  upb
  absl::flat_hash_map
  absl::strings)
target_link_libraries(file_layout_stage0 PUBLIC
  common_stage0
  reflection_stage0
  descriptor_upb_proto_stage0)

add_library(file_layout_stage1 
    ../upbc/file_layout.cc
  ../upbc/file_layout.h
)
target_include_directories(file_layout_stage1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(file_layout_stage1 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(file_layout_stage1 PUBLIC
  mini_table
  mini_table_internal
  port
  upb
  absl::flat_hash_map
  absl::strings)
target_link_libraries(file_layout_stage1 PUBLIC
  common_stage1
  reflection_stage1
  descriptor_upb_proto_stage1)

add_library(file_layout 
    ../upbc/file_layout.cc
  ../upbc/file_layout.h
)
target_include_directories(file_layout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(file_layout PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(file_layout PUBLIC
  mini_table
  mini_table_internal
  port
  upb
  absl::flat_hash_map
  absl::strings)
target_link_libraries(file_layout PUBLIC
  common
  reflection
  descriptor_upb_proto)
endif()

add_library(keywords 
    ../upbc/keywords.cc
  ../upbc/keywords.h
)
target_include_directories(keywords PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(keywords PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
if (UPB_ENABLE_CODEGEN)

add_library(plugin_stage0 INTERFACE
    
)
target_include_directories(plugin_stage0 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin_stage0 INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(plugin_stage0 INTERFACE
  port
  absl::flat_hash_set
  absl::absl_check
  absl::absl_log
  absl::strings)
target_link_libraries(plugin_stage0 INTERFACE
  plugin_upb_proto_stage0
  descriptor_upb_proto_stage0
  reflection_stage0)

add_library(plugin_stage1 INTERFACE
    
)
target_include_directories(plugin_stage1 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin_stage1 INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(plugin_stage1 INTERFACE
  port
  absl::flat_hash_set
  absl::absl_check
  absl::absl_log
  absl::strings)
target_link_libraries(plugin_stage1 INTERFACE
  plugin_upb_proto_stage1
  descriptor_upb_proto_stage1
  reflection_stage1)

add_library(plugin INTERFACE
    
)
target_include_directories(plugin INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(plugin INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(plugin INTERFACE
  port
  absl::flat_hash_set
  absl::absl_check
  absl::absl_log
  absl::strings)
target_link_libraries(plugin INTERFACE
  plugin_upb_proto
  descriptor_upb_proto
  reflection)
endif()
if (UPB_ENABLE_CODEGEN)

add_library(names_stage0 
    ../upbc/names.cc
  ../upbc/names.h
)
target_include_directories(names_stage0 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(names_stage0 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(names_stage0 PUBLIC
  absl::core_headers
  absl::flat_hash_map
  absl::strings
  protobuf::libprotobuf
  protobuf::libprotoc)
target_link_libraries(names_stage0 PUBLIC
  reflection_stage0)

add_library(names_stage1 
    ../upbc/names.cc
  ../upbc/names.h
)
target_include_directories(names_stage1 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(names_stage1 PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(names_stage1 PUBLIC
  absl::core_headers
  absl::flat_hash_map
  absl::strings
  protobuf::libprotobuf
  protobuf::libprotoc)
target_link_libraries(names_stage1 PUBLIC
  reflection_stage1)

add_library(names 
    ../upbc/names.cc
  ../upbc/names.h
)
target_include_directories(names PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(names PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(names PUBLIC
  absl::core_headers
  absl::flat_hash_map
  absl::strings
  protobuf::libprotobuf
  protobuf::libprotoc)
target_link_libraries(names PUBLIC
  reflection)
endif()
if (UPB_ENABLE_CODEGEN)

add_executable(protoc-gen-upb_stage0
    ../upbc/protoc-gen-upb.cc
)
target_include_directories(protoc-gen-upb_stage0 PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(protoc-gen-upb_stage0 PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(protoc-gen-upb_stage0 PRIVATE
  base
  mem
  mini_table_internal
  port
  wire_types
  absl::flat_hash_map
  absl::flat_hash_set
  absl::absl_check
  absl::absl_log
  absl::strings)
target_link_libraries(protoc-gen-upb_stage0 PRIVATE
  common_stage0
  file_layout_stage0
  names_stage0
  plugin_stage0
  plugin_upb_proto_stage0
  descriptor_upb_proto_stage0
  reflection_stage0)

add_executable(protoc-gen-upb_stage1
    ../upbc/protoc-gen-upb.cc
)
target_include_directories(protoc-gen-upb_stage1 PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(protoc-gen-upb_stage1 PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(protoc-gen-upb_stage1 PRIVATE
  base
  mem
  mini_table_internal
  port
  wire_types
  absl::flat_hash_map
  absl::flat_hash_set
  absl::absl_check
  absl::absl_log
  absl::strings)
target_link_libraries(protoc-gen-upb_stage1 PRIVATE
  common_stage1
  file_layout_stage1
  names_stage1
  plugin_stage1
  plugin_upb_proto_stage1
  descriptor_upb_proto_stage1
  reflection_stage1)

add_executable(protoc-gen-upb
    ../upbc/protoc-gen-upb.cc
)
target_include_directories(protoc-gen-upb PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(protoc-gen-upb PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(protoc-gen-upb PRIVATE
  base
  mem
  mini_table_internal
  port
  wire_types
  absl::flat_hash_map
  absl::flat_hash_set
  absl::absl_check
  absl::absl_log
  absl::strings)
target_link_libraries(protoc-gen-upb PRIVATE
  common
  file_layout
  names
  plugin
  plugin_upb_proto
  descriptor_upb_proto
  reflection)
endif()
if (UPB_ENABLE_CODEGEN)

add_executable(protoc-gen-upbdefs
    ../upbc/protoc-gen-upbdefs.cc
)
target_include_directories(protoc-gen-upbdefs PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINRARY_DIR}>
)
if(NOT UPB_ENABLE_CODEGEN)
  target_include_directories(protoc-gen-upbdefs PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../cmake>
  )
endif()
target_link_libraries(protoc-gen-upbdefs PRIVATE
  common
  file_layout
  plugin
  descriptor_upb_proto
  reflection
  def_to_proto)
endif()


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
  port upb base mini_table mini_table_internal message message_internal message_accessors_internal message_accessors message_promote message_copy message_split64 fastdecode generated_code_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me generated_cpp_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me generated_reflection_support__only_for_generated_code_do_not_use__i_give_permission_to_break_me collections collections_internal collections_split64 textformat json mem mem_internal wire wire_internal wire_types eps_copy_input_stream wire_reader hash lex def_to_proto required_fields compare keywords
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
    descriptor_upb_proto descriptor_upb_proto_defs reflection reflection_internal plugin_upb_proto plugin_upb_proto_defs common file_layout plugin names protoc-gen-upb protoc-gen-upbdefs
    ${UPB_CODEGEN_TARGETS}
    EXPORT upb-config
  )
endif()
install(EXPORT upb-config NAMESPACE upb:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/upb")

