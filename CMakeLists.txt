cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# --- Find required packages ------------------------------------------------
find_package(FgLib5 REQUIRED)
find_package(ClSerMe4 REQUIRED)

include_directories(${FGLIB5_INCLUDE_DIR} ${CLSERME4_INCLUDE_DIR})

add_definitions("--std=c99 -Wall")

# --- Build library ---------------------------------------------------------
add_library(pco SHARED libpco.c)

# --- Build test executable -------------------------------------------------
add_executable(test main.c)
target_link_libraries(test pco ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})

