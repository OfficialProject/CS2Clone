cmake_minimum_required(VERSION 3.25)
project(CS2Clone VERSION 0.1.0 LANGUAGES CXX)

option(CS2CLONE_BUILD_TESTS "Build the headless simulation tests" ON)
option(CS2CLONE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(game_simulation src/simulation/movement.cpp)
target_include_directories(game_simulation PUBLIC include)
target_compile_features(game_simulation PUBLIC cxx_std_23)

function(cs2clone_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${CS2CLONE_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
            $<$<BOOL:${CS2CLONE_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
endfunction()
cs2clone_configure_target(game_simulation)

add_executable(movement_lab tools/movement_lab/main.cpp)
target_link_libraries(movement_lab PRIVATE game_simulation)
cs2clone_configure_target(movement_lab)

if(CS2CLONE_BUILD_TESTS)
    enable_testing()
    add_executable(simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(simulation_tests PRIVATE game_simulation)
    cs2clone_configure_target(simulation_tests)
    add_test(NAME simulation_tests COMMAND simulation_tests)
endif()

option(CS2CLONE_BUILD_PLAYTEST "Build the interactive 3D movement feel lab" OFF)
if(CS2CLONE_BUILD_PLAYTEST)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(raylib)
    add_executable(feel_lab tools/feel_lab/main.cpp)
    target_link_libraries(feel_lab PRIVATE game_simulation raylib)
    cs2clone_configure_target(feel_lab)
endif()
