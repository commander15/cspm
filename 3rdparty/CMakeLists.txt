set(json_version 3.12.0)
set(json_schema_validator_version 2.4.1)
set(qeloquent_version 1.1.0)

# Global vars
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static lib build" FORCE)
set(BUILD_TESTING     OFF)

# json vars
set(nlohmann_json_VERSION ${json_version})
set(JSON_VALIDATOR_INSTALL        OFF)
set(JSON_VALIDATOR_BUILD_TESTS    OFF)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF)
set(JSON_VALIDATOR_SHARED_LIBS    OFF)
set(JSON_VALIDATOR_TEST_COVERAGE  OFF)

# QEloquent vars
set(QELOQUENT_BUILD_DOC OFF)
set(QELOQUENT_BUILD_EXAMPLES OFF)
set(QELOQUENT_BUILD_TESTS OFF)

function(fetch_dependency dir sub_dir version repo tag)
    set(archive ${CMAKE_BINARY_DIR}/tmp/${dir}_${tag}.zip)

    if (NOT EXISTS ${archive})
        file(DOWNLOAD ${repo}/archive/refs/tags/${tag}.zip ${archive} SHOW_PROGRESS)
    endif()

    if (NOT EXISTS ${dir})
        file(ARCHIVE_EXTRACT INPUT ${archive} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
    endif()

    add_subdirectory(${dir}/${sub_dir}-${version})
endfunction()

fetch_dependency(
    nlohmann json ${json_version}
    https://github.com/nlohmann/json v${json_version}
)

fetch_dependency(
    pboettch json-schema-validator ${json_schema_validator_version}
    https://github.com/pboettch/json-schema-validator ${json_schema_validator_version}
)

fetch_dependency(
    commander15 QEloquent ${qeloquent_version}
    https://github.com/commander15/QEloquent v${qeloquent_version}
)