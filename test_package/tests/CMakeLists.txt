cmake_minimum_required(VERSION 3.14.0)

find_package(upb REQUIRED)

upb_generate_defs(
  PROJECT_UPBDEF_SRCS
  PROJECT_UPBDEF_HDRS
  ../../tests/empty.proto
  ../../tests/test_cpp.proto
  ../../tests/test.proto
)
set_source_files_properties(${PROJECT_UPBDEF_SRCS} ${PROJECT_UPBDEF_HDRS} PROPERTIES GENERATED TRUE)

upb_generate_c(
  PROJECT_PROTO_SRCS
  PROJECT_PROTO_HDRS
  ../../tests/empty.proto
  ../../tests/test_cpp.proto
  ../../tests/test.proto
)
set_source_files_properties(${PROJECT_PROTO_SRCS} ${PROJECT_PROTO_HDRS} PROPERTIES GENERATED TRUE)

if(NOT CMAKE_CROSSCOMPILING)
  add_executable(test_cpp)
  target_sources(
    test_cpp
    PUBLIC ../../tests/testmain.cc
           ../../tests/test_cpp.cc
           ${PROJECT_PROTO_SRCS}
           ${PROJECT_PROTO_HDRS}
           ${PROJECT_UPBDEF_SRCS}
           ${PROJECT_UPBDEF_HDRS}
  )
  target_include_directories(
    test_cpp PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
                    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(test_cpp PUBLIC upb::upb upb::json)
  target_compile_options(test_cpp PRIVATE $<$<CXX_COMPILER_ID:MSVC>:>)

  # # test_messages_proto3 from protocolbuffers/protobuf required, skipping for now
  # add_executable(test_generated_code)
  # target_sources(
  #   test_generated_code
  #   PRIVATE ../../tests/testmain.cc
  #           ../../tests/test_generated_code.c
  #           ${PROJECT_PROTO_SRCS}
  #           ${PROJECT_PROTO_HDRS}
  #           ${PROJECT_UPBDEF_SRCS}
  #           ${PROJECT_UPBDEF_HDRS}
  # )
  # target_include_directories(
  #   test_generated_code PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  #                              $<INSTALL_INTERFACE:include>
  # )
  # target_link_libraries(test_generated_code PUBLIC upb::upb)
  # target_compile_options(test_generated_code PRIVATE $<$<CXX_COMPILER_ID:MSVC>:>)

  add_executable(test_table)
  target_sources(
    test_table
    PRIVATE ../../tests/testmain.cc
            ../../tests/test_table.cc
            ${PROJECT_PROTO_SRCS}
            ${PROJECT_PROTO_HDRS}
            ${PROJECT_UPBDEF_SRCS}
            ${PROJECT_UPBDEF_HDRS}
  )
  target_include_directories(
    test_table PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/..> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
                      $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(test_table PUBLIC upb::upb)
  target_compile_options(test_table PRIVATE $<$<CXX_COMPILER_ID:MSVC>:>)
endif()
