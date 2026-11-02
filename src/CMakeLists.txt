set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

qt_add_executable(cspm main.cpp
    core/command.h core/command_p.h core/command.cpp
    pm/package.cpp pm/package.h
    pm/steps/packagestep.h pm/steps/packagestep.cpp
    pm/steps/packagefetchstep.h pm/steps/packagefetchstep.cpp
    build/buildcommand.cpp build/buildcommand.h
    build/packagebuilder.h build/packagebuilder.cpp
    build/steps/packagecmakestep.h build/steps/packagecmakestep.cpp
    utils/json.cpp utils/json.h
    utils/locator.h utils/locator.cpp
    pm/commands/updatecommand.cpp pm/commands/updatecommand.h
    pm/commands/installcommand.cpp pm/commands/installcommand.h
    pm/commands/removecommand.cpp pm/commands/removecommand.h
    pm/commands/upgradecommand.h pm/commands/upgradecommand.cpp
    pm/packagemanager.h pm/packagemanager.cpp
    utils/http.h utils/http.cpp
    utils/string.h utils/string.cpp
)

target_compile_definitions(cspm PRIVATE
    CSPM_VERSION="${PROJECT_VERSION}"
    CSPM_REMOTE="https://cdn.jsdelivr.net/gh/commander15/commander-registry@main"
)

target_link_libraries(cspm PRIVATE Qt6::Core Qt6::Network)