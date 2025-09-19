add_executable(test_initgetset 1_initgetset.c)
target_link_libraries(test_initgetset  memkv)

add_executable(test_memcap 2_memcap.c)
target_link_libraries(test_memcap  memkv)

add_executable(test_triekv triekv.c)
target_link_libraries(test_triekv  memkv)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(test_initgetset PRIVATE ENABLE_LOG)
    target_compile_definitions(test_memcap PRIVATE ENABLE_LOG)
    target_compile_definitions(test_triekv PRIVATE ENABLE_LOG)
endif()