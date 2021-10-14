# This file was generated from BUILD using tools/make_cmakelists.py.

cmake_minimum_required(VERSION 3.1)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
  cmake_policy(VERSION 3.12)
endif()

cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0048 NEW)

project(
  upb
  VERSION 0.0.0
  LANGUAGES C CXX
)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_PROJECT_NAME} STREQUAL upb AND EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup()
endif()

# Prevent CMake from setting -rdynamic on Linux (!!).
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

# Set default build type.
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE
  )
endif()

# When using Ninja, compiler output won't be colorized without this.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-fdiagnostics-color=always SUPPORTS_COLOR_ALWAYS)
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

if(APPLE)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup -flat_namespace")
elseif(UNIX)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--build-id")
endif()

enable_testing()

set(UPB_LIBS)
set(UPB_INTERFACES)

add_library(port INTERFACE)
list(APPEND UPB_INTERFACES port)

add_library(upb)
target_sources(
  upb
  PRIVATE ../upb/decode.c
          ../upb/decode_internal.h
          ../upb/encode.c
          ../upb/msg.c
          ../upb/msg_internal.h
          ../upb/table.c
          ../upb/table_internal.h
          ../upb/upb.c
          ../upb/upb_internal.h
)
target_include_directories(upb PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>)
target_link_libraries(upb PUBLIC fastdecode port)
list(APPEND UPB_LIBS upb)

add_library(fastdecode)
target_sources(
  fastdecode
  PRIVATE ../upb/decode_internal.h
          ../upb/decode_fast.c
          ../upb/msg_internal.h
          ../upb/upb_internal.h
)
target_include_directories(fastdecode PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>)
target_link_libraries(fastdecode PUBLIC port table)
list(APPEND UPB_LIBS fastdecode)

add_library(descriptor_upb_proto)
target_sources(descriptor_upb_proto PRIVATE google/protobuf/descriptor.upb.c)
target_include_directories(
  descriptor_upb_proto PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}> $<INSTALL_INTERFACE:include>
)
target_link_libraries(descriptor_upb_proto PUBLIC port table upb)
list(APPEND UPB_LIBS descriptor_upb_proto)

add_library(reflection)
target_sources(reflection PRIVATE ../upb/def.c ../upb/reflection.c)
target_include_directories(
  reflection PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                    $<INSTALL_INTERFACE:include>
)
target_link_libraries(

  reflection
  PUBLIC descriptor_upb_proto
         port
         table
         upb
)
list(APPEND UPB_LIBS reflection)

add_library(textformat)
target_sources(textformat PRIVATE ../upb/text_encode.c)
target_include_directories(textformat PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>)
target_link_libraries(textformat PUBLIC port reflection)
list(APPEND UPB_LIBS textformat)

add_library(json)
target_sources(json PRIVATE ../upb/json_decode.c ../upb/json_encode.c)
target_include_directories(json PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>)
target_link_libraries(json PUBLIC port reflection upb)
list(APPEND UPB_LIBS json)

add_library(table INTERFACE)
target_link_libraries(table INTERFACE port)
list(APPEND UPB_INTERFACES table)



if(NOT CMAKE_CROSSCOMPILING)
  find_package(Protobuf REQUIRED)
  find_package(absl REQUIRED)

  add_library(common)
  target_sources(common PRIVATE ../upbc/common.cc)
  target_include_directories(common PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>)
  target_link_libraries(common PUBLIC absl::base absl::strings)
  list(APPEND UPB_LIBS common)

  add_executable(protoc-gen-upb)
  target_sources(protoc-gen-upb PRIVATE ../upbc/protoc-gen-upb.cc ../upbc/message_layout.cc)
  target_include_directories(
    protoc-gen-upb PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(
    protoc-gen-upb
    PUBLIC common
           absl::flat_hash_map
           protobuf::libprotoc
           protobuf::libprotobuf
  )

  add_executable(protoc-gen-upbdefs)
  target_sources(protoc-gen-upbdefs PRIVATE ../upbc/protoc-gen-upbdefs.cc)
  target_include_directories(
    protoc-gen-upbdefs PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(protoc-gen-upbdefs PUBLIC common protobuf::libprotoc protobuf::libprotobuf)
  set(UPB_EXECUTABLES protoc-gen-upb protoc-gen-upbdefs)
endif()

include(InstallHelpers.cmake)
export_package(
  NAMESPACE
  upb
  LIBS
  ${UPB_LIBS}
  INTERFACE_LIBS
  ${UPB_INTERFACES}
  EXECUTABLES
  ${UPB_EXECUTABLES}
)
# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

