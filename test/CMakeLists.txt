project(test)
cmake_minimum_required(VERSION 3.2)
set(CMAKE_CXX_STANDARD 17)
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src")
set(PROJECT_TEST_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/test/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_TEST_BINARY_DIR})

set(test_list)
list(APPEND test_list test_fib)
list(APPEND test_list test_path_open)
foreach(test ${test_list})
    message("add test file: ${test}")
    add_executable(${test} ${CMAKE_SOURCE_DIR}/test/${test}/${test}.cpp)
    target_link_libraries(${test} binaryen wasm_instrumenter_lib)
    target_include_directories(${test} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/binaryen/src)
endforeach()