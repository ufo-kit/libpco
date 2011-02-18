cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

find_package(FgLib5 REQUIRED)
find_package(ClSerMe4 REQUIRED)

include_directories(${FGLIB5_INCLUDE_DIR} ${CLSERME4_INCLUDE_DIR})

add_definitions("--std=c99 -Wall")
add_executable(test main.c)
target_link_libraries(test ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})

