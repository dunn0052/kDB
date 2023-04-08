cmake_minimum_required(VERSION 3.16)
project(Schema)

set( SRC src )
set( INC inc )


set(CXXSRC ${SRC}/main.cpp ${SRC}/schema.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )

target_include_directories(${PROJECT_NAME} PRIVATE
  ${INC} ${COMMON_INCLUDE} )

target_compile_definitions(${PROJECT_NAME} PRIVATE
  __LOG_ENABLE
  #__LOG_SHOW_LINE 
  __LOG_SHOW_COLOR )

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND Schema -a
)
