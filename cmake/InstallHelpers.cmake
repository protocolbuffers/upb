function(export_package)
  set(prefix EXPORT_PACKAGE)
  set(options OPTIONS)
  set(oneValueArgs NAMESPACE PACKAGE_NAME)
  set(multiValueArgs LIBS INTERFACE_LIBS EXECUTABLES)
  cmake_parse_arguments(
    PARSE_ARGV
    0
    "${prefix}"
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
  )
  if(NOT DEFINED ${prefix}_PACKAGE_NAME)
    set(${prefix}_PACKAGE_NAME ${${prefix}_NAMESPACE})
  endif()

  # Install cmake modules
  include(GNUInstallDirs)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Modules)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Modules DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
  endif()

  foreach(target ${${prefix}_LIBS})
    add_library(${${prefix}_NAMESPACE}::${target} ALIAS ${target})
  endforeach()
  foreach(target ${${prefix}_INTERFACE_LIBS})
    add_library(${${prefix}_NAMESPACE}::${target} ALIAS ${target})
  endforeach()
  foreach(target ${${prefix}_EXECUTABLES})
    add_executable(${${prefix}_NAMESPACE}::${target} ALIAS ${target})
  endforeach()

  set(_targets)
  list(
    APPEND
    _targets
    ${${prefix}_LIBS}
    ${${prefix}_INTERFACE_LIBS}
    ${${prefix}_EXECUTABLES}
  )
  export_stub(${${prefix}_NAMESPACE} ${${prefix}_PACKAGE_NAME} "${_targets}")
  unset(_targets)
endfunction()

function(
  export_stub
  namespace
  target
  targets
)
  # ############################################################################
  # Installation instructions
  include(GNUInstallDirs)
  set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${target})

  install(DIRECTORY ${CMAKE_SOURCE_DIR}/../upb
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING
          PATTERN "*.h"
          PATTERN "*.hpp"
          PATTERN "*.inc")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/../upbc
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING
          PATTERN "*.h"
          PATTERN "*.hpp"
          PATTERN "*.inc")

  install(DIRECTORY ${CMAKE_SOURCE_DIR}/google DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install(
    TARGETS ${targets}
    EXPORT ${target}-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # Export the targets to a script
  install(
    EXPORT ${target}-targets
    FILE ${target}Targets.cmake
    NAMESPACE ${namespace}::
    DESTINATION ${INSTALL_CONFIGDIR}
  )

  # Create a ConfigVersion.cmake file
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
  )
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/${target}Config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
  )

  # Install the config, configversion and custom find modules
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake DESTINATION ${INSTALL_CONFIGDIR}
  )
  # ############################################################################
  # Exporting from the build tree
  export(
    EXPORT ${target}-targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/${target}Targets.cmake
    NAMESPACE ${namespace}::
  )

  # Register package in user's package registry
  export(PACKAGE ${target})
endfunction()
