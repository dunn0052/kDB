project(CLI)

set( SRC src )
set( INC inc )

include_directories( ${INC} ${INC}/../../common_inc ${INC}/../../Logger/inc)


set(CXXSRC ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )
target_link_libraries(${PROJECT_NAME} PUBLIC Logger)