cmake_minimum_required(VERSION 3.13.4)
project(db)

set(COMMON_INCLUDE ./common_inc)

add_subdirectory(Logger)
add_subdirectory(CLI)
add_subdirectory(Schema)
add_subdirectory(DBMapper)

