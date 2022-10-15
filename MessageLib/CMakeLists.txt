project(MessageLib)

set( SRC src )
set( INC inc )

include_directories( ${INC} ${COMMON_INCLUDE} ${DB_INCLUDE} )

add_compile_definitions(_ENABLE_LOGGING)

set(CXXSRC ${SRC}/Messenger.cpp )

add_library(${PROJECT_NAME} ${CXXSRC} )

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
