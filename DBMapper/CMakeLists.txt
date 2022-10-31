project(DBMapper)

set( SRC src )

include_directories( ${COMMON_INCLUDE} ${DB_INCLUDE} )

add_compile_definitions(_ENABLE_LOGGING)

set(CXXSRC ${SRC}/Database.cpp ${SRC}/main.cpp )

#add_executable(${PROJECT_NAME}  ${CXXSRC} )
#target_link_libraries(${PROJECT_NAME} PUBLIC Logger)
#install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

add_library(${PROJECT_NAME} ${SRC}/Database.cpp )

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
