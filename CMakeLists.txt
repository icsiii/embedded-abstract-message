cmake_minimum_required(VERSION 3.9)
project(test_message C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_MACOSX_RPATH 0)

set(CBOR_PARSER_NO_STRICT_CHECKS TRUE)
include_directories(vendor/tinycbor/src)
add_subdirectory(vendor/tinycbor/src)

set(ENABLE_CJSON_TEST OFF)
include_directories(vendor/cJSON)
add_subdirectory(vendor/cJSON)

add_executable(test_message main.c ts_message.c)
target_link_libraries(test_message tinycbor cjson)
