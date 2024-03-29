cmake_minimum_required(VERSION 3.2)
project(wasm_instrumenter)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src sources)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/binaryen)

add_executable(wasm_instrumenter ${sources})

set(BUILD_STATIC_LIBS ON)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_OBJECT_LIBS OFF)

target_link_libraries(wasm_instrumenter binaryen)
target_include_directories(wasm_instrumenter SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/binaryen/src)