cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# --- Find required packages ------------------------------------------------
find_package(FgLib5 REQUIRED)
find_package(ClSerMe4 REQUIRED)

include_directories(${FGLIB5_INCLUDE_DIR} ${CLSERME4_INCLUDE_DIR})

add_definitions("--std=c99 -Wall")

# --- Build library and install ---------------------------------------------
add_library(pco SHARED libpco.c)

install(TARGETS pco
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)

install(FILES
    libpco.h 
    sc2_cl.h
    sc2_add.h
    sc2_command.h
    sc2_common.h
    sc2_telegram.h
    sc2_drv_struct.h
    PCO_err.h
    
    DESTINATION include/libpco)

# --- Build test executable -------------------------------------------------
add_executable(diagnose main.c reorder_func.c)
target_link_libraries(diagnose pco ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})

