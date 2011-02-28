cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# --- Find required packages ------------------------------------------------
find_package(ClSerMe4 REQUIRED)
find_package(FgLib5)

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

set(CPACK_PACKAGE_DESCRIPTION "User-space device access to pco cameras")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "User-space device access to pco cameras")
set(CPACK_PACKAGE_NAME "libpco")

# --- Distro specific
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.6), libgcc1 (>= 1:4.1)")
set(CPACK_SET_DESTDIR ON)

set(CPACK_PACKAGE_CONTACT "Matthias Vogelgesang")
set(CPACK_PACKAGE_VENDOR "Karlsruhe Institute of Technology/IPE")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_GENERATOR "DEB;RPM;")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)

# --- Build test executable -------------------------------------------------
if (FGLIB5_FOUND)
    add_executable(diagnose test/main.c)
    target_link_libraries(diagnose pco ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})
endif()
