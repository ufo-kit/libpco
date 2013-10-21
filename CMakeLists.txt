cmake_minimum_required(VERSION 2.6)

set(TARNAME "libpco")
set(LIBPCO_VERSION_MAJOR "0")
set(LIBPCO_VERSION_MINOR "4")
set(LIBPCO_VERSION_PATCH "1")
set(LIBPCO_VERSION_STRING "${LIBPCO_VERSION_MAJOR}.${LIBPCO_VERSION_MINOR}.${LIBPCO_VERSION_PATCH}")

set(LIBPCO_ABI_VERSION "${LIBPCO_VERSION_MAJOR}.${LIBPCO_VERSION_MINOR}")

set(PACKAGE_VERSION "${LIBPCO_VERSION_STRING}")
set(PACKAGE_NAME "${TARNAME}")
set(PACKAGE_TARNAME "${TARNAME}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "http://ufo.kit.edu/ufo/newticket")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

set(LIBPCO_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --- Find required packages ------------------------------------------------
find_package(ClSerMe4 REQUIRED)
find_package(FgLib5)

include_directories(${CMAKE_SOURCE_DIR}/src 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${FGLIB5_INCLUDE_DIR} 
    ${CLSERME4_INCLUDE_DIR})

add_definitions("--std=c99 -Wall -fpack-struct")

# --- Build library and install ---------------------------------------------
add_library(pco SHARED src/libpco.c)
target_link_libraries(pco ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})
set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

set_target_properties(pco PROPERTIES
      VERSION ${LIBPCO_ABI_VERSION}
      SOVERSION ${LIBPCO_VERSION_MINOR})

configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)

install(TARGETS pco
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})

install(FILES
    src/libpco.h 
    src/sc2_defs.h
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
set(CPACK_PACKAGE_VERSION_MAJOR "${LIBPCO_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${LIBPCO_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${LIBPCO_VERSION_PATCH}")
set(VERSION "${LIBPCO_VERSION_STRING}")

set(CPACK_GENERATOR "TGZ;DEB;RPM;")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "tags" ".bzr")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "libpco-${LIBPCO_VERSION_STRING}" CACHE INTERNAL "tarball basename")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}-${CMAKE_SYSTEM_PROCESSOR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libpco.spec.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libpco.spec" @ONLY IMMEDIATE)

include(CPack)

# --- Documentation ---------------------------------------------------------
find_package(Doxygen)

if(DOXYGEN_FOUND)
    option(WITH_DOCUMENTATION "Enable documentation" ON)
    if (WITH_DOCUMENTATION)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        add_custom_command(TARGET pco POST_BUILD
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen" VERBATIM)
    endif()
endif()

# --- Build test executable -------------------------------------------------
if (FGLIB5_FOUND)
    add_executable(diagnose test/main.c)
    target_link_libraries(diagnose pco ${FGLIB5_LIBRARY} ${CLSERME4_LIBRARY})
endif()

# --- pkg-config --------------------------------------------------------------
set(PCO_PKG_PREFIX ${CMAKE_INSTALL_PREFIX})
set(PCO_PKG_EXEC_PREFIX "${PCO_PKG_PREFIX}/bin")
set(PCO_PKG_LIBDIR "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
set(PCO_PKG_INCLUDEDIR "${PCO_PKG_PREFIX}/include")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/pco.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pco.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pco.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
