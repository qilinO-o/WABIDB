cmake_minimum_required(VERSION 3.2)
project(wasm_instrumenter)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src sources)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/binaryen)
foreach(SUFFIX "_DEBUG" "_RELEASE" "")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${SUFFIX} "${PROJECT_BINARY_DIR}/bin")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${SUFFIX} "${PROJECT_BINARY_DIR}/lib")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY${SUFFIX} "${PROJECT_BINARY_DIR}/lib")
endforeach()

add_executable(wasm_instrumenter ${sources})

#set(BUILD_STATIC_LIBS ON)
set(BUILD_SHARED_LIBS ON)
#set(BUILD_OBJECT_LIBS ON)
add_library(wasm_instrumenter_lib ${sources})

target_link_libraries(wasm_instrumenter binaryen)
target_include_directories(wasm_instrumenter SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/binaryen/src)
target_link_libraries(wasm_instrumenter_lib binaryen)
target_include_directories(wasm_instrumenter_lib SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/binaryen/src)
#target_compile_options(wasm_instrumenter PRIVATE -O3)

add_subdirectory(test)
enable_testing()
add_test(test_fib ${PROJECT_BINARY_DIR}/test/test_fib)
add_test(test_path_open ${PROJECT_BINARY_DIR}/test/test_path_open)

add_subdirectory(src/tools)

add_subdirectory(examples)