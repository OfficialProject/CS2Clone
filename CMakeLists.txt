cmake_minimum_required(VERSION 3.25)
project(CS2Clone VERSION 0.1.0 LANGUAGES CXX)

option(CS2CLONE_BUILD_TESTS "Build the headless simulation tests" ON)
option(CS2CLONE_BUILD_JOLT_LAB "Build the experimental Jolt collision laboratory" OFF)
option(CS2CLONE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(game_simulation
    src/simulation/movement.cpp
)
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

if(CS2CLONE_BUILD_JOLT_LAB)
    include(FetchContent)

    # Pin the exact tested release. The lab remains optional so the core
    # movement simulation can build without a network fetch or third party.
    set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "Jolt cross-platform deterministic build" FORCE)
    set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "Build Jolt as a static library" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use the same dynamic MSVC runtime as the engine" FORCE)
    set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Use the engine's platform optimization flags" FORCE)
    set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "Disable unused Jolt ObjectStream support" FORCE)
    set(JPH_USE_DX12 OFF CACHE BOOL "Disable unused Jolt DirectX compute support" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "Disable unused Jolt Vulkan compute support" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "Disable unused Jolt Metal compute support" FORCE)
    set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "Disable unused Jolt compute support" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable Jolt LTO in the experiment" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Use the engine's warning policy for its own code" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "Do not install the experiment dependency" FORCE)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Disable unused Jolt debug renderer" FORCE)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Disable unused Jolt profiler" FORCE)
    set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "Skip debug symbols for the dependency experiment" FORCE)
    set(USE_SSE4_1 OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_SSE4_2 OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_AVX OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_AVX2 OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_AVX512 OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_LZCNT OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_TZCNT OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_F16C OFF CACHE BOOL "Keep the Jolt baseline compatible with SSE2 CPUs" FORCE)
    set(USE_FMADD OFF CACHE BOOL "Avoid FMA in the deterministic Jolt build" FORCE)

    FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.6.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(JoltPhysics)

    add_executable(collision_lab tools/collision_lab/main.cpp)
    target_link_libraries(collision_lab PRIVATE Jolt::Jolt)
    target_compile_features(collision_lab PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(collision_lab PRIVATE /W4 /fp:strict)
    else()
        target_compile_options(collision_lab PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
    endif()
    if(CS2CLONE_BUILD_TESTS)
        add_test(NAME collision_lab COMMAND collision_lab)
    endif()
endif()
