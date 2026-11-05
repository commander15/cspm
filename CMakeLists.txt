cmake_minimum_required(VERSION 3.30)

project(cspm VERSION 1.0.0)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

include(FetchContent)

FetchContent_Declare(
    json-schema-validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG 2.4.1
    GIT_PROGRESS
    FIND_PACKAGE_ARGS NAMES nlohmann_json_schema_validator 2.4.1
)

set(JSON_VALIDATOR_INSTALL        OFF)
set(JSON_VALIDATOR_BUILD_TESTS    OFF)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF)
set(JSON_VALIDATOR_SHARED_LIBS    OFF)
set(JSON_VALIDATOR_TEST_COVERAGE  OFF)

FetchContent_MakeAvailable(json-schema-validator)

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