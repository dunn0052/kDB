#cmake_minimum_required(VERSION 3.13.4)

project(Schema)

set( SRC src )
set( INC inc )

include_directories( ${INC} ${INC}/../../common_inc ${INC}/../../CLI/inc ${INC}/../../Logger/inc)

add_compile_definitions( _LOG_DEBUG )
set(CXXSRC ${SRC}/main.cpp ${SRC}/schema.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )
target_link_libraries(${PROJECT_NAME} PUBLIC Logger)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
