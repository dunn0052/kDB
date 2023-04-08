project(DBDebug)

set( SRC src )
set( INC inc )


set(CXXSRC ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )

target_include_directories(${PROJECT_NAME} PRIVATE
  ${INC} ${COMMON_INCLUDE} ${DB_INCLUDE})

target_compile_definitions(${PROJECT_NAME} PRIVATE
  __LOG_ENABLE
  __LOG_SHOW_COLOR )

add_dependencies(${PROJECT_NAME}
  "Schema")

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND DBDebug -a
)
