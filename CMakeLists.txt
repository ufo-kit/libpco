cmake_minimum_required(VERSION 2.6)
project(libpco C)

#{{{ CMake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

include(GNUInstallDirs)
set(CMAKE_INSTALL_PKGCONFIGDIR  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
#}}}
#{{{ Metadata
set(TARNAME "libpco")
set(LIBPCO_VERSION_MAJOR "1")
set(LIBPCO_VERSION_MINOR "0")
set(LIBPCO_VERSION_PATCH "0")
set(LIBPCO_VERSION_STRING "${LIBPCO_VERSION_MAJOR}.${LIBPCO_VERSION_MINOR}.${LIBPCO_VERSION_PATCH}")
set(LIBPCO_ABI_VERSION ${LIBPCO_VERSION_MAJOR})
set(LIBPCO_VERSION "${LIBPCO_VERSION_MAJOR}.${LIBPCO_VERSION_MINOR}")
set(LIBPCO_DESCRIPTION "User-space device access to pco cameras")
#}}}
#{{{ Dependencies
find_package(ClSerSis REQUIRED)
find_package(FgLib5 REQUIRED)
find_package(Doxygen)
#}}}
#{{{ Targets
include_directories(${CMAKE_SOURCE_DIR}/src 
                    ${CMAKE_CURRENT_BINARY_DIR}
                    ${FgLib5_INCLUDE_DIR}
                    ${clsersis_INCLUDE_DIR})

add_definitions("--std=c99 -Wall -fpack-struct")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/pco.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/pco.pc" @ONLY IMMEDIATE)

configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_library(pco SHARED src/libpco.c)

target_link_libraries(pco ${FgLib5_LIBRARY} ${clsersis_LIBRARY})

set_target_properties(pco PROPERTIES
                      VERSION "${LIBPCO_VERSION_MAJOR}.${LIBPCO_VERSION_MINOR}"
                      SOVERSION ${LIBPCO_VERSION_MAJOR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pco.pc
        DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR})

install(TARGETS pco
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES src/libpco.h src/sc2_defs.h src/PCO_err.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libpco)

add_executable(diagnose test/main.c)
target_link_libraries(diagnose pco ${FgLib5_LIBRARY} ${clsersis_LIBRARY})
#}}}
#{{{ Documentation
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
#}}}
#}}}
#{{{ CPack
set(CPACK_PACKAGE_NAME "${TARNAME}")
set(CPACK_PACKAGE_DESCRIPTION ${LIBPCO_DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${LIBPCO_DESCRIPTION})

set(PACKAGE_NAME "${TARNAME}")
set(PACKAGE_TARNAME "${TARNAME}")
set(PACKAGE_VERSION "${LIBPCO_VERSION_STRING}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "http://ufo.kit.edu/ufo/newticket")

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
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_SOURCE_PACKAGE_FILE_NAME "libpco-${LIBPCO_VERSION_STRING}" CACHE INTERNAL "tarball basename")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${LIBPCO_VERSION_STRING}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

set(LIBPCO_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libpco.spec.in"
               "${CMAKE_CURRENT_BINARY_DIR}/libpco.spec" @ONLY IMMEDIATE)
set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_BINARY_DIR}/libpco.spec")

include(CPack)
#}}}
