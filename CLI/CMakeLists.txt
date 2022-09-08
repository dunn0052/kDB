project(CLI)

set( SRC src )

include_directories( ${COMMON_INCLUDE} )


set(CXXSRC ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )
target_link_libraries(${PROJECT_NAME} PUBLIC Logger)