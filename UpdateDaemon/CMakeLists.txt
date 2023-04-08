cmake_minimum_required(VERSION 3.16)
project(UpdateDaemon)

set( SRC src )
set( INC inc )


set(CXXSRC ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )

target_include_directories(${PROJECT_NAME} PRIVATE
  ${INC} ${COMMON_INCLUDE} ${DB_INCLUDE} )

target_compile_definitions(${PROJECT_NAME} PRIVATE
  __LOG_ENABLE
  __LOG_SHOW_TIME
  __INET_BLACKLIST )

add_dependencies(${PROJECT_NAME}
  "DBMapper"
  "Schema")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)