cmake_minimum_required(VERSION 3.16)
Project("DBMapper")

add_library(${PROJECT_NAME} SHARED
  "src/Database.cpp"
  )
add_dependencies(${PROJECT_NAME}
    "Schema")

target_include_directories(${PROJECT_NAME} PRIVATE ${COMMON_INCLUDE} ${DB_INCLUDE})

target_compile_definitions(${PROJECT_NAME} PRIVATE
  __LOG_ENABLE
  __LOG_SHOW_LINE )

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION lib)