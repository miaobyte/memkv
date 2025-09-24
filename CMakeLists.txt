cmake_minimum_required(VERSION 3.10)
project(memkv)

set(CMAKE_C_STANDARD 11)

# 添加头文件路径
include_directories(include)
include_directories(src)

# 使用 FetchContent 下载和引入 block_malloc
include(FetchContent)
FetchContent_Declare(
    block_malloc
    GIT_REPOSITORY https://github.com/miaobyte/block_malloc.git
    GIT_TAG main
)
FetchContent_MakeAvailable(block_malloc)
FetchContent_Declare(
    box_malloc
    GIT_REPOSITORY https://github.com/miaobyte/box_malloc.git
    GIT_TAG main
)
FetchContent_MakeAvailable(box_malloc)

include_directories(${box_malloc_SOURCE_DIR}/include)
include_directories(${block_malloc_SOURCE_DIR}/include)

# 添加源文件
add_library(memkv SHARED
    src/memkv.c
    src/miaobyte.c
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(memkv PRIVATE ENABLE_LOG) # 在 Debug 模式下定义 ENABLE_LOG
endif()
target_link_libraries(memkv box_malloc block_malloc)

add_executable(miaobytecli src/miaobytecli.c)
target_link_libraries(miaobytecli memkv)

add_subdirectory(test)

# 安装目标和头文件
install(TARGETS memkv DESTINATION lib/memkv)
install(DIRECTORY include/ DESTINATION include/memkv)
install(TARGETS miaobytecli DESTINATION bin)