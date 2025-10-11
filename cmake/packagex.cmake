# Minimal packaging helper for memkv: write package config, export targets.

if(NOT DEFINED PROJECT_NAME)
  message(FATAL_ERROR "PROJECT_NAME is not defined. Call project(...) before including PackageMemkv.cmake")
endif()
if(NOT DEFINED PROJECT_VERSION)
  message(FATAL_ERROR "PROJECT_VERSION is not defined. Call project(... VERSION x.y.z) before including PackageMemkv.cmake")
endif()

include(CMakePackageConfigHelpers)

set(MEMKV_PKG_NAME "${PROJECT_NAME}")
set(MEMKV_PKG_VERSION "${PROJECT_VERSION}")

# generate the config file from template
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/MemkvConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${MEMKV_PKG_NAME}Config.cmake"
  @ONLY
)

# version file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${MEMKV_PKG_NAME}ConfigVersion.cmake"
  VERSION ${MEMKV_PKG_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# Install exported targets (ensure you used install(EXPORT memkvTargets ...))
install(EXPORT memkvTargets
    FILE "${MEMKV_PKG_NAME}Targets.cmake"
    NAMESPACE ${MEMKV_PKG_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${MEMKV_PKG_NAME}-${MEMKV_PKG_VERSION}
)

# Install the generated config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${MEMKV_PKG_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${MEMKV_PKG_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${MEMKV_PKG_NAME}-${MEMKV_PKG_VERSION}
)

# Debian packaging will control package metadata; no CPack configuration here.