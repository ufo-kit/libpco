cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# --- Find required packages ------------------------------------------------
find_package(FgLib5 REQUIRED)
find_package(ClSerMe4 REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src ${FGLIB5_INCLUDE_DIR} ${CLSERME4_INCLUDE_DIR})

add_definitions("--std=c99 -Wall")

# --- Build library and install ---------------------------------------------
add_library(pco SHARED src/libpco.c)

install(TARGETS pco
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)

install(FILES
    src/libpco.h 
    src/sc2_cl.h
    src/sc2_add.h
    src/sc2_command.h
    src/sc2_common.h
    src/sc2_telegram.h
    src/sc2_drv_struct.h
    src/PCO_err.h
    
    DESTINATION include/libpco)

# --- Build test executable -------------------------------------------------
add_executable(diagnose test/main.c)
target_link_libraries(diagnose pco ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})

