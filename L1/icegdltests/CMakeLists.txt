################################################################################
# Westeros-Sink - ICEGDL Variant
################################################################################
cmake_minimum_required(VERSION 3.10)
project(westeros_l1_icegdl_tests C CXX)

set(CMAKE_CXX_STANDARD 11)

# ICEGDL-specific definitions
add_definitions(
    -D_FILE_OFFSET_BITS=64
    -DUSE_GST1
    -DUSE_GST_VIDEO
    -DUSE_GST_ALLOCATORS
    -DNATIVE_BUILD
    -DVERSION='\"1.0.0\"'
    -DPACKAGE_NAME='\"westeros-sink\"'
    -DUNIT_TEST_BUILD
)

# Find dependencies
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0)
pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-server)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../icegdl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../mocks
    ${GSTREAMER_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${DRM_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    /usr/include/gstreamer-1.0
    /usr/include/glib-2.0
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
    /usr/include/libdrm
    /usr/local/include
)

# Source files for ICEGDL variant
set(WESTEROS_ICEGDL_SOURCES
    ../../westeros-sink.c
    ../../pipeline_logger.cpp
    ../../icegdl/westeros-sink-soc.c
)

set(MOCK_SOURCES
    ../mocks/essos_resmgr_stubs.c
    ../mocks/wayland_stubs.c
    ../mocks/gst_dmabuf_stubs.c
    ../mocks/platform_stubs.c
    ../mocks/ismd_stubs.c
    ../mocks/gdl_stubs.c
)

# L1 test sources (single test file per variant)
file(GLOB L1_ICEGDL_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Create executable
add_executable(L1_test_icegdl
    ${WESTEROS_ICEGDL_SOURCES}
    ${L1_ICEGDL_TESTS}
    ${MOCK_SOURCES}
)

# Explicit coverage compilation flags (for Ubuntu 24.04 GCC 11+ compatibility)
target_compile_options(L1_test_icegdl PRIVATE
    -g -O0 -fprofile-arcs -ftest-coverage --coverage
)

# Link libraries
target_link_libraries(L1_test_icegdl
    ${GTEST_BOTH_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${DRM_LIBRARIES}
    ${GLIB_LIBRARIES}
    westeros_simpleshell_client
    westeros_simpleshell_server
    westeros_compositor
    gbm
    EGL
    GLESv2
    dl
    pthread
)

# Coverage linker flags
if(ENABLE_COVERAGE)
    target_link_options(L1_test_icegdl PRIVATE ${COVERAGE_LINK_FLAGS} -Wl,--no-as-needed -lgcov -Wl,--as-needed)
else()
    target_link_options(L1_test_icegdl PRIVATE --coverage)
endif()

# Register test with CTest
add_test(NAME L1_test_icegdl COMMAND L1_test_icegdl)