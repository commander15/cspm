cmake_minimum_required(VERSION 3.30)

project(cspm VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

# QTP0003: ensure BUILD_SHARED_LIBS is respected on Qt 6.7 and later
if (${Qt6_VERSION} VERSION_GREATER_EQUAL 6.7.0)
    qt_policy(SET QTP0003 NEW)
endif()

add_subdirectory(3rdparty)
add_subdirectory(src)

set(CPACK_PACKAGE_NAME "cspm")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A serverless, CDN-based C++ package manager")
set(CPACK_PACKAGE_VENDOR "Amadou Benjamain")
set(CPACK_PACKAGE_CONTACT "Amadou Benjamain <amadoubenjamain@gmail.com>")

set(CPACK_GENERATOR ZIP)

if (UNIX)
    list(APPEND CPACK_GENERATOR TGZ DEB)

    set(DEBIAN_DEPS
        "libqt6core6t64 (>= 6.8.0)"
        "libqt6network6 (>= 6.8.0)"
        "git"
        "7zip"
        "cmake (>= 3.30.0)"
    )
    string(JOIN ", " CPACK_DEBIAN_PACKAGE_DEPENDS ${DEBIAN_DEPS})
endif()

include(CPack)