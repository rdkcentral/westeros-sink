################################################################################
# L1 Test Suite for Westeros-Sink - Top-Level CMakeLists.txt
# 
# Builds L1 tests for all 7 westeros-sink variants:
# brcm, drm, emu, icegdl, raw, rpi, v4l2
#
# Features:
# - Builds one L1 test binary per variant
# - Enables code coverage for each variant
# - Supports coverage merging across all variants
################################################################################

cmake_minimum_required(VERSION 3.10)
project(westeros_sink_l1_tests C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project metadata
set(PROJECT_DESCRIPTION "L1 Unit Test Suite for Westeros-Sink GStreamer Plugin")
set(PROJECT_VERSION 1.0.0)

# Print banner
message(STATUS "Westeros-Sink L1 Test Suite v${PROJECT_VERSION}")

################################################################################
# Build Options
################################################################################
option(BUILD_BRCM_TESTS "Build BRCM variant L1 tests" ON)
option(BUILD_DRM_TESTS "Build DRM variant L1 tests" ON)
option(BUILD_EMU_TESTS "Build EMU variant L1 tests" ON)
option(BUILD_ICEGDL_TESTS "Build ICEGDL variant L1 tests" ON)
option(BUILD_RAW_TESTS "Build RAW variant L1 tests" ON)
option(BUILD_RPI_TESTS "Build RPI variant L1 tests" ON)
option(BUILD_V4L2_TESTS "Build V4L2 variant L1 tests (includes main westeros-sink tests)" ON)
option(ENABLE_COVERAGE "Enable code coverage analysis" ON)

################################################################################
# Compiler Configuration
################################################################################
# Base compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wextra")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(COVERAGE_COMPILE_FLAGS --coverage -O0 -g)
    set(COVERAGE_LINK_FLAGS --coverage)
else()
    set(COVERAGE_COMPILE_FLAGS "")
    set(COVERAGE_LINK_FLAGS "")
endif()

if(ENABLE_COVERAGE)
    message(STATUS "✓ Code coverage enabled")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

################################################################################
# Find Required Packages
################################################################################
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)

# GStreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0)

# Wayland
pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-server)

# DRM
pkg_check_modules(DRM REQUIRED libdrm)

# GLib
pkg_check_modules(GLIB REQUIRED glib-2.0)

################################################################################
# Source Root Directory
################################################################################
# The source files are in the parent directory
set(WESTEROS_SINK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Verify required source files exist
if(NOT EXISTS "${WESTEROS_SINK_SOURCE_DIR}/westeros-sink.c")
    message(FATAL_ERROR "westeros-sink.c not found in ${WESTEROS_SINK_SOURCE_DIR}")
endif()

if(NOT EXISTS "${WESTEROS_SINK_SOURCE_DIR}/westeros-sink.h")
    message(FATAL_ERROR "westeros-sink.h not found in ${WESTEROS_SINK_SOURCE_DIR}")
endif()

if(NOT EXISTS "${WESTEROS_SINK_SOURCE_DIR}/pipeline_logger.cpp")
    message(FATAL_ERROR "pipeline_logger.cpp not found in ${WESTEROS_SINK_SOURCE_DIR}")
endif()

################################################################################
# Enable Testing
################################################################################
enable_testing()

################################################################################
# Add Variant Test Subdirectories
################################################################################
if(BUILD_BRCM_TESTS)
    add_subdirectory(brcmtests)
endif()

if(BUILD_DRM_TESTS)
    add_subdirectory(drmtests)
endif()

if(BUILD_EMU_TESTS)
    add_subdirectory(emutests)
endif()

if(BUILD_ICEGDL_TESTS)
    add_subdirectory(icegdltests)
endif()

if(BUILD_RAW_TESTS)
    add_subdirectory(rawtests)
endif()

if(BUILD_RPI_TESTS)
    add_subdirectory(rpitests)
endif()

if(BUILD_V4L2_TESTS)
    add_subdirectory(v4l2tests)
endif()

################################################################################
# Coverage Report Target
################################################################################
if(ENABLE_COVERAGE)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        message(STATUS "✓ Coverage tools found")
        
        add_custom_target(coverage
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/generate_coverage.sh"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Generating coverage report.."
        )
    else()
        message(WARNING "⚠ Coverage tools not found. Install: sudo apt-get install lcov genhtml")
    endif()
endif()

################################################################################
# Build Summary
################################################################################
message(STATUS "Westeros-Sink L1 Test Suite configured successfully")