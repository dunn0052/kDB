project(DBMapper)

set( SRC src )
set( INC inc )

include_directories( ${INC} ${INC}/../../common_inc ${INC}/../../db/inc ${INC}/../../CLI/inc ${INC}/../../Logger/inc)

add_compile_definitions(_ENABLE_LOGGING)

set(CXXSRC ${SRC}/Database.cpp ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )
target_link_libraries(${PROJECT_NAME} PUBLIC Logger)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
