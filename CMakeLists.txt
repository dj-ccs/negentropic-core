# CMakeLists.txt - negentropic-core Build System
#
# Build targets:
#   - negentropic_core: Shared library (.dll/.so/.dylib)
#   - negentropic_core_static: Static library (.a/.lib)
#   - integrator_smoke_test: Pre-flight test executable
#   - WASM: WebAssembly module (via emscripten)
#
# Build options:
#   - NEGENTROPIC_CORE_ENABLED: ON/OFF (kill switch for Unity fallback)
#   - BUILD_WASM: ON/OFF (enable WebAssembly target)
#   - BUILD_TESTS: ON/OFF (build test executables)
#
# Author: negentropic-core team
# Version: 0.1.0
# License: MIT OR GPL-3.0

cmake_minimum_required(VERSION 3.15)

project(negentropic-core
    VERSION 0.1.0
    DESCRIPTION "Deterministic physics kernel for Negentropic ecosystem"
    LANGUAGES C
)

# ========================================================================
# OPTIONS
# ========================================================================

option(NEGENTROPIC_CORE_ENABLED "Enable negentropic-core (kill switch)" ON)
option(BUILD_WASM "Build WebAssembly module" OFF)
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_SHARED_LIBS "Build shared library" ON)

if(NOT NEGENTROPIC_CORE_ENABLED)
    message(WARNING "NEGENTROPIC_CORE_ENABLED=OFF - Core library disabled (fallback mode)")
    return()
endif()

# ========================================================================
# COMPILER SETTINGS
# ========================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        add_compile_options(-O3 -march=native -ffast-math)
    endif()
endif()

# ========================================================================
# SOURCE FILES
# ========================================================================

set(CORE_SOURCES
    src/core/state.c
    src/core/neg_error.c
    src/core/rng.c
    src/api/negentropic.c
    embedded/se3_math.c
    embedded/trig_tables.c
    embedded/handoff.c
    embedded/t_bsp.c
)

set(CORE_HEADERS
    src/core/state.h
    src/core/include/state_versioning.h
    src/core/include/neg_error.h
    src/core/include/rng.h
    src/core/include/se3_types.h
    src/core/include/platform.h
    src/api/negentropic.h
    embedded/se3_edge.h
    embedded/t_bsp.h
)

# ========================================================================
# LIBRARY TARGETS
# ========================================================================

if(BUILD_SHARED_LIBS)
    add_library(negentropic_core SHARED ${CORE_SOURCES})
    set_target_properties(negentropic_core PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
        PUBLIC_HEADER "${CORE_HEADERS}"
    )
    target_include_directories(negentropic_core PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

# Static library (always build for Unity P/Invoke)
add_library(negentropic_core_static STATIC ${CORE_SOURCES})
set_target_properties(negentropic_core_static PROPERTIES
    OUTPUT_NAME negentropic_core
    PUBLIC_HEADER "${CORE_HEADERS}"
)
target_include_directories(negentropic_core_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core/include>
    $<INSTALL_INTERFACE:include>
)

# Math library (on Unix)
if(UNIX AND NOT APPLE)
    if(BUILD_SHARED_LIBS)
        target_link_libraries(negentropic_core PRIVATE m)
    endif()
    target_link_libraries(negentropic_core_static PRIVATE m)
endif()

# ========================================================================
# TEST TARGETS
# ========================================================================

if(BUILD_TESTS)
    enable_testing()

    # Smoke test
    add_executable(integrator_smoke_test
        tests/integrator_smoke_test.c
        embedded/se3_math.c
        embedded/trig_tables.c
    )
    target_include_directories(integrator_smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    if(UNIX AND NOT APPLE)
        target_link_libraries(integrator_smoke_test PRIVATE m)
    endif()

    add_test(NAME IntegratorSmokeTest COMMAND integrator_smoke_test)

    # RNG determinism test
    add_executable(rng_test
        tests/rng_test.c
        src/core/rng.c
    )
    target_include_directories(rng_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME RNGTest COMMAND rng_test)

    # Existing fixed-point test
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixed_point_accuracy_test.c")
        add_executable(fixed_point_test
            tests/fixed_point_accuracy_test.c
            embedded/se3_math.c
            embedded/trig_tables.c
        )
        target_include_directories(fixed_point_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

        if(UNIX AND NOT APPLE)
            target_link_libraries(fixed_point_test PRIVATE m)
        endif()

        add_test(NAME FixedPointTest COMMAND fixed_point_test)
    endif()
endif()

# ========================================================================
# WASM TARGET (Emscripten)
# ========================================================================

if(BUILD_WASM)
    if(EMSCRIPTEN)
        add_executable(negentropic_core_wasm ${CORE_SOURCES})

        set_target_properties(negentropic_core_wasm PROPERTIES
            OUTPUT_NAME "negentropic_core"
            SUFFIX ".wasm"
        )

        target_compile_options(negentropic_core_wasm PRIVATE
            -O3
            -s WASM=1
            -s STRICT=1
            -s ALLOW_MEMORY_GROWTH=0
            -s INITIAL_MEMORY=16MB
        )

        target_link_options(negentropic_core_wasm PRIVATE
            -O3
            -s WASM=1
            -s STRICT=1
            -s ALLOW_MEMORY_GROWTH=0
            -s EXPORTED_FUNCTIONS='["_neg_create","_neg_step","_neg_get_state_json","_neg_get_state_binary","_neg_get_state_hash","_neg_reset_from_binary","_neg_destroy"]'
            -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
        )

        target_include_directories(negentropic_core_wasm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    else()
        message(WARNING "BUILD_WASM=ON but not using Emscripten. Skipping WASM target.")
    endif()
endif()

# ========================================================================
# INSTALLATION
# ========================================================================

include(GNUInstallDirs)

if(BUILD_SHARED_LIBS)
    install(TARGETS negentropic_core
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/negentropic
    )
endif()

install(TARGETS negentropic_core_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/negentropic
)

# ========================================================================
# PACKAGE CONFIG
# ========================================================================

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/negentropic-core-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/negentropic-core-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/negentropic-core
)

# ========================================================================
# SUMMARY
# ========================================================================

message(STATUS "")
message(STATUS "negentropic-core v${PROJECT_VERSION} Configuration:")
message(STATUS "  NEGENTROPIC_CORE_ENABLED: ${NEGENTROPIC_CORE_ENABLED}")
message(STATUS "  BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "  BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "  BUILD_WASM: ${BUILD_WASM}")
message(STATUS "  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
