cmake_minimum_required(VERSION 3.13.4)
project(db)

set(COMMON_INCLUDE ./common_inc)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(Logger)
add_subdirectory(CLI)
add_subdirectory(Schema)
add_subdirectory(DBMapper)

