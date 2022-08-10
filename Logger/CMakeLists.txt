
project(Logger)

set( SRC src )
set( INC inc )

include_directories( ${INC} ${INC}/../../common_inc)

set(CXXSRC ${SRC}/Logger.cpp )

add_library(${PROJECT_NAME} ${CXXSRC} )

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})