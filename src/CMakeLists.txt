set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

qt_add_executable(cspm main.cpp
    core/command.h core/command.cpp
    core/command_p.h
    build/buildcommand.cpp build/buildcommand.h
    build/packagebuilder.h build/packagebuilder.cpp
    utils/json.cpp utils/json.h
    utils/toollocator.h utils/toollocator.cpp
    build/steps/packagefetchstep.h build/steps/packagefetchstep.cpp
    core/packagestep.h core/packagestep.cpp
    core/package.cpp core/package.h
    build/steps/packagecmakestep.h build/steps/packagecmakestep.cpp)

add_subdirectory(core)
add_subdirectory(build)

target_compile_definitions(cspm PRIVATE
    CSPM_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(cspm PRIVATE Qt6::Core Qt6::Network)