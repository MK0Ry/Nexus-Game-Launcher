cmake_minimum_required(VERSION 3.24)

project(NexusGameLauncher
    VERSION 0.1.0
    DESCRIPTION "A local-first Windows game library and launcher"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Sql)

qt_standard_project_setup()

set(NEXUS_SOURCES
    src/main.cpp
    src/app/app_settings.cpp
    src/data/database.cpp
    src/data/game_repository.cpp
    src/launcher/game_launcher.cpp
    src/model/game.cpp
    src/ui/game_dialog.cpp
    src/ui/main_window.cpp
)

set(NEXUS_HEADERS
    include/nexus/app/app_settings.h
    include/nexus/data/database.h
    include/nexus/data/game_repository.h
    include/nexus/launcher/game_launcher.h
    include/nexus/model/game.h
    include/nexus/ui/game_dialog.h
    include/nexus/ui/main_window.h
)

qt_add_executable(nexus-launcher WIN32 MACOSX_BUNDLE
    ${NEXUS_SOURCES}
    ${NEXUS_HEADERS}
)

target_include_directories(nexus-launcher PRIVATE include)
target_link_libraries(nexus-launcher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

target_compile_definitions(nexus-launcher PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

if(MSVC)
    target_compile_options(nexus-launcher PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(nexus-launcher PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    qt_add_executable(nexus-launcher-tests
        tests/test_nexus.cpp
        src/app/app_settings.cpp
        src/data/database.cpp
        src/data/game_repository.cpp
        src/launcher/game_launcher.cpp
        src/model/game.cpp
        include/nexus/app/app_settings.h
        include/nexus/data/database.h
        include/nexus/data/game_repository.h
        include/nexus/launcher/game_launcher.h
        include/nexus/model/game.h
    )
    target_include_directories(nexus-launcher-tests PRIVATE include)
    target_link_libraries(nexus-launcher-tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Test
    )
    if(MSVC)
        target_compile_options(nexus-launcher-tests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(nexus-launcher-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nexus-launcher-tests COMMAND nexus-launcher-tests)
endif()

install(TARGETS nexus-launcher
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
