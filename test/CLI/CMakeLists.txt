project(CLI_Tests)


add_executable(
  ${PROJECT_NAME}
  TEST_CLI.cpp
)
target_link_libraries(
  ${PROJECT_NAME}
  GTest::gtest_main
)

include(GoogleTest)
target_include_directories(${PROJECT_NAME} PRIVATE ${COMMON_INCLUDE})


gtest_discover_tests(${PROJECT_NAME})