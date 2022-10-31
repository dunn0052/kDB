
project(Logger)

set( SRC src )

include_directories( ${COMMON_INCLUDE} )

set(CXXSRC ${SRC}/Logger.cpp )

add_library(${PROJECT_NAME} ${CXXSRC} )

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})