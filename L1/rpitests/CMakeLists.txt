################################################################################
# Westeros-Sink - RPI Variant
################################################################################
cmake_minimum_required(VERSION 3.10)
project(westeros_l1_rpi_tests C CXX)

set(CMAKE_CXX_STANDARD 11)

# RPI-specific definitions
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../rpi
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

# Source files for RPI variant
set(WESTEROS_RPI_SOURCES
    ../../westeros-sink.c
    ../../pipeline_logger.cpp
    ../../rpi/westeros-sink-soc.c
)

set(MOCK_SOURCES
    ../mocks/essos_resmgr_stubs.c
    ../mocks/wayland_stubs.c
    ../mocks/gst_dmabuf_stubs.c
    ../mocks/platform_stubs.c
)

# L1 test sources (single test file per variant)
file(GLOB L1_RPI_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Create executable
add_executable(L1_test_rpi
    ${WESTEROS_RPI_SOURCES}
    ${L1_RPI_TESTS}
    ${MOCK_SOURCES}
)

# Explicit coverage compilation flags (for Ubuntu 24.04 GCC 11+ compatibility)
target_compile_options(L1_test_rpi PRIVATE
    -g -O0 -fprofile-arcs -ftest-coverage --coverage
)

# Link libraries
target_link_libraries(L1_test_rpi
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

# Coverage linker flags with EGL function wrapping
if(ENABLE_COVERAGE)
    target_link_options(L1_test_rpi PRIVATE ${COVERAGE_LINK_FLAGS} -Wl,--no-as-needed -lgcov -Wl,--as-needed
        -Wl,--wrap=eglGetDisplay
        -Wl,--wrap=eglInitialize
        -Wl,--wrap=eglTerminate
        -Wl,--wrap=eglReleaseThread
        -Wl,--wrap=eglMakeCurrent
        -Wl,--wrap=eglGetCurrentContext
        -Wl,--wrap=eglGetCurrentDisplay
        -Wl,--wrap=eglGetCurrentSurface
        -Wl,--wrap=eglSwapBuffers
        -Wl,--wrap=eglSwapInterval
        -Wl,--wrap=eglQueryString
        -Wl,--wrap=eglGetConfigs
        -Wl,--wrap=eglChooseConfig
        -Wl,--wrap=eglGetConfigAttrib
        -Wl,--wrap=eglCreateContext
        -Wl,--wrap=eglDestroyContext
        -Wl,--wrap=eglCreateWindowSurface
        -Wl,--wrap=eglCreatePbufferSurface
        -Wl,--wrap=eglDestroySurface
        -Wl,--wrap=eglGetError
        -Wl,--wrap=glXDestroyContext
        -Wl,--wrap=glXDestroyWindow
        -Wl,--wrap=glXMakeCurrent
        -Wl,--wrap=glDeleteProgram
        -Wl,--wrap=glDeleteShader
        -Wl,--wrap=glDeleteBuffers
        -Wl,--wrap=glDeleteTextures
        -Wl,--wrap=glDeleteFramebuffersEXT
        -Wl,--wrap=glDeleteRenderbuffersEXT
        -Wl,--wrap=glDeleteVertexArrays
        -Wl,--wrap=drmClose
        -Wl,--wrap=gbm_device_destroy
        -Wl,--wrap=gbm_surface_destroy
    )
else()
    target_link_options(L1_test_rpi PRIVATE --coverage
        -Wl,--wrap=eglGetDisplay
        -Wl,--wrap=eglInitialize
        -Wl,--wrap=eglTerminate
        -Wl,--wrap=eglReleaseThread
        -Wl,--wrap=eglMakeCurrent
        -Wl,--wrap=eglGetCurrentContext
        -Wl,--wrap=eglGetCurrentDisplay
        -Wl,--wrap=eglGetCurrentSurface
        -Wl,--wrap=eglSwapBuffers
        -Wl,--wrap=eglSwapInterval
        -Wl,--wrap=eglQueryString
        -Wl,--wrap=eglGetConfigs
        -Wl,--wrap=eglChooseConfig
        -Wl,--wrap=eglGetConfigAttrib
        -Wl,--wrap=eglCreateContext
        -Wl,--wrap=eglDestroyContext
        -Wl,--wrap=eglCreateWindowSurface
        -Wl,--wrap=eglCreatePbufferSurface
        -Wl,--wrap=eglDestroySurface
        -Wl,--wrap=eglGetError
        -Wl,--wrap=glXDestroyContext
        -Wl,--wrap=glXDestroyWindow
        -Wl,--wrap=glXMakeCurrent
        -Wl,--wrap=glDeleteProgram
        -Wl,--wrap=glDeleteShader
        -Wl,--wrap=glDeleteBuffers
        -Wl,--wrap=glDeleteTextures
        -Wl,--wrap=glDeleteFramebuffersEXT
        -Wl,--wrap=glDeleteRenderbuffersEXT
        -Wl,--wrap=glDeleteVertexArrays
        -Wl,--wrap=drmClose
        -Wl,--wrap=gbm_device_destroy
        -Wl,--wrap=gbm_surface_destroy
    )
endif()

# Register test with CTest
add_test(NAME L1_test_rpi COMMAND L1_test_rpi)