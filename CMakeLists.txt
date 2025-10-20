cmake_minimum_required(VERSION 3.10)
project(memkv VERSION 0.1.0)   # 设置版本，dpkg-buildpackage 会使用 debian/changelog 版本

set(CMAKE_C_STANDARD 11)
include(GNUInstallDirs)

# 添加头文件路径
include_directories(include)
include_directories(src)

include(FetchContent)
find_package(blockmalloc CONFIG QUIET)
if(blockmalloc_FOUND)
    message(STATUS "Found blockmalloc package")
else()
    message(STATUS "blockmalloc package not found, fetching from GitHub")
    FetchContent_Declare(
        blockmalloc
        GIT_REPOSITORY https://github.com/miaobyte/blockmalloc.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(blockmalloc)
endif()

find_package(boxmalloc CONFIG QUIET)
if(boxmalloc_FOUND)
    message(STATUS "Found boxmalloc package")
else()
    message(STATUS "boxmalloc package not found, fetching from GitHub")
    FetchContent_Declare(
        boxmalloc
        GIT_REPOSITORY https://github.com/miaobyte/boxmalloc.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(boxmalloc)
endif()

# 添加源文件
add_library(memkv SHARED
    src/memkv.c
    src/miaobyte.c
)

# 统一解析依赖目标名（兼容 find_package 与 FetchContent）
if(TARGET boxmalloc::boxmalloc)
    set(_boxmalloc_target boxmalloc::boxmalloc)
elseif(TARGET boxmalloc)
    add_library(boxmalloc::boxmalloc ALIAS boxmalloc)
    set(_boxmalloc_target boxmalloc::boxmalloc)
else()
    message(FATAL_ERROR "boxmalloc target not found")
endif()

if(TARGET blockmalloc::blockmalloc)
    set(_blockmalloc_target blockmalloc::blockmalloc)
elseif(TARGET blockmalloc)
    add_library(blockmalloc::blockmalloc ALIAS blockmalloc)
    set(_blockmalloc_target blockmalloc::blockmalloc)
else()
    message(FATAL_ERROR "blockmalloc target not found")
endif()

# 给 memkv 设置版本/SOVERSION，而不是 boxmalloc
set_target_properties(memkv PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# include 路径也应设置到 memkv
target_include_directories(memkv
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(memkv PRIVATE ENABLE_LOG)
endif()

# 正确链接命名空间目标
target_link_libraries(memkv PUBLIC ${_boxmalloc_target} ${_blockmalloc_target})

add_executable(miaobyte src/miaobytecli.c)
target_link_libraries(miaobyte memkv)

add_subdirectory(test)


# 安装目标和头文件（使用 GNUInstallDirs 变量，便于打包）
install(TARGETS memkv
    EXPORT memkvTargets
    LIBRARY DESTINATION ${CMAKE_CURRENT_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_CURRENT_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_CURRENT_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS miaobyte DESTINATION ${CMAKE_INSTALL_BINDIR})


# Generate CMake config package files for downstream consumers
include("${CMAKE_CURRENT_LIST_DIR}/cmake/packagex.cmake")