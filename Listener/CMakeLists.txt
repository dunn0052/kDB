project(Listener)

set( SRC src )

include_directories( ${COMMON_INCLUDE} )

add_compile_definitions(_ENABLE_LOGGING)

set(CXXSRC  ${SRC}/main.cpp )

add_executable(${PROJECT_NAME}  ${CXXSRC} )

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Logger PRIVATE Threads::Threads)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
