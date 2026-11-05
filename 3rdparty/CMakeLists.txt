include(FetchContent)

set(json_version 3.12.0)
set(json_schema_validator_version 2.4.1)

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

return()

# ============ nlohmann/json =================

if (NOT EXISTS ${CMAKE_BINARY_DIR}/tmp/nlohmann_json.zip)
    file(
        DOWNLOAD "https://github.com/nlohmann/json/archive/refs/tags/v${json_version}.zip"
        ${CMAKE_BINARY_DIR}/tmp/nlohmann_json.zip
        SHOW_PROGRESS
    )
endif()

file(ARCHIVE_EXTRACT
    INPUT ${CMAKE_BINARY_DIR}/tmp/nlohmann_json.zip
    DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann
)

add_subdirectory(nlohmann/json-${json_version})

# ============ pboettch/json-schema-validator =================

FetchContent_Declare(
    json-schema-validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG ${json_schema_validator_version}
    GIT_PROGRESS
    # FIND_PACKAGE_ARGS NAMES nlohmann_json_schema_validator 2.4.1
)

set(nlohmann_json_VERSION ${json_version})
set(JSON_VALIDATOR_INSTALL        OFF)
set(JSON_VALIDATOR_BUILD_TESTS    OFF)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF)
set(JSON_VALIDATOR_SHARED_LIBS    OFF)
set(JSON_VALIDATOR_TEST_COVERAGE  OFF)

FetchContent_MakeAvailable(json-schema-validator)
