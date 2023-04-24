cmake_minimum_required(VERSION 3.16)

# MACROS for public includes
set(COMMON_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/common_inc)
set(DB_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/db/inc)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE True
  INTERPROCEDURAL_OPTIMIZATION False )

project("DB")
add_subdirectory(Schema)
add_subdirectory(DBMapper)
add_subdirectory(DBSet)
add_subdirectory(DBDebug)
add_subdirectory(InstantiateDB)
add_subdirectory(Listener)
add_subdirectory(UpdateDaemon)

add_subdirectory(test)
