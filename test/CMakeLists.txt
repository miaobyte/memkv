add_executable(test_triekv triekv.c)
target_link_libraries(test_triekv memkv)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(test_triekv PRIVATE ENABLE_LOG)
endif()