project(DBMapper)

set( SRC src )
set( INC inc )

include_directories( ${INC} ${INC}/../../common_inc ${INC}/../../db/inc )


set(CXXSRC ${SRC}/Database.cpp ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
