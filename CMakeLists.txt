cmake_minimum_required(VERSION 3.25)
project(CS2Clone VERSION 0.1.0 LANGUAGES CXX)

option(CS2CLONE_BUILD_TESTS "Build the headless simulation tests" ON)
option(CS2CLONE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(game_simulation src/simulation/movement.cpp)
target_include_directories(game_simulation PUBLIC include)
target_compile_features(game_simulation PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(game_simulation PRIVATE /W4 $<$<BOOL:${CS2CLONE_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(game_simulation PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
        $<$<BOOL:${CS2CLONE_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

add_executable(movement_lab tools/movement_lab/main.cpp)
target_link_libraries(movement_lab PRIVATE game_simulation)

if(CS2CLONE_BUILD_TESTS)
    enable_testing()
    add_executable(simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(simulation_tests PRIVATE game_simulation)
    add_test(NAME simulation_tests COMMAND simulation_tests)
endif()
