cmake_minimum_required(VERSION 3.13.4)
project(db)

# MACROS for public includes
set(COMMON_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/common_inc)
set(DB_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/db/inc)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(Logger)
add_subdirectory(CLI)
add_subdirectory(Schema)
add_subdirectory(DBMapper)
#add_subdirectory(MessageLib)
add_subdirectory(Talker)
add_subdirectory(Listener)

