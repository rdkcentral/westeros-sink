################################################################################
# Westeros-Sink - V4L2 Variant
################################################################################
cmake_minimum_required(VERSION 3.10)
project(westeros_l1_v4l2_tests C CXX)

set(CMAKE_CXX_STANDARD 11)

# V4L2-specific definitions
add_compile_definitions(
    UNIT_TEST_BUILD
    _FILE_OFFSET_BITS=64
    USE_GST1
    USE_GST_VIDEO
    USE_GST_ALLOCATORS
    NATIVE_BUILD
    USE_GST_AFD
    ENABLE_SW_DECODE
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=default -DUNIT_TEST_BUILD -DVERSION='\"1.0.0\"' -DPACKAGE_NAME='\"westeros-sink\"'")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=default -DUNIT_TEST_BUILD -DVERSION='\"1.0.0\"' -DPACKAGE_NAME='\"westeros-sink\"'")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../v4l2
    ${CMAKE_CURRENT_SOURCE_DIR}/../../v4l2/svp
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

# Source files for V4L2 variant
set(WESTEROS_V4L2_SOURCES
    ../../westeros-sink.c
    ../../pipeline_logger.cpp
    ../../v4l2/westeros-sink-soc.c
)

set(MOCK_SOURCES
    ../mocks/gstreamer_stubs.c
    ../mocks/essos_resmgr_stubs.c
    ../mocks/wayland_stubs.c
    ../mocks/gst_dmabuf_stubs.c
    ../mocks/platform_stubs.c

)

# L1 test sources 
file(GLOB L1_V4L2_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Create executable
add_executable(L1_test_v4l2
    ${WESTEROS_V4L2_SOURCES}
    ${L1_V4L2_TESTS}
    ${MOCK_SOURCES}
)

target_compile_options(L1_test_v4l2 PRIVATE 
    -DUNIT_TEST_BUILD
    -fvisibility=default
    -fPIC
    -fprofile-arcs
    -ftest-coverage
)

# Ensure all symbols are exported from the executable
# Explicit coverage and debug flags
target_compile_options(L1_test_v4l2 PRIVATE
    -g -O0 -fprofile-arcs -ftest-coverage --coverage -rdynamic
)

# Link libraries
target_link_libraries(L1_test_v4l2
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
    avcodec
    avutil
)

# Coverage linker flags with GStreamer/GLib function wrapping
if(ENABLE_COVERAGE)
    target_link_options(L1_test_v4l2 PRIVATE ${COVERAGE_LINK_FLAGS} -Wl,--no-as-needed -lgcov -Wl,--as-needed
        -rdynamic
        -Wl,--allow-multiple-definition
        -Wl,--export-dynamic
        -Wl,--wrap=gst_base_sink_set_sync
        -Wl,--wrap=gst_base_sink_set_async_enabled
        -Wl,--wrap=gst_base_sink_is_async_enabled
        -Wl,--wrap=gst_buffer_peek_memory
        -Wl,--wrap=gst_element_class_set_static_metadata
        -Wl,--wrap=g_object_class_install_property
        -Wl,--wrap=g_object_class_override_property
        -Wl,--wrap=g_signal_new
        -Wl,--wrap=gst_caps_from_string
        -Wl,--wrap=gst_caps_new_simple
        -Wl,--wrap=gst_caps_unref
        -Wl,--wrap=gst_pad_template_new
        -Wl,--wrap=gst_static_pad_template_get
        -Wl,--wrap=gst_element_class_add_pad_template
        -Wl,--wrap=gst_type_find_factory_get_list
        -Wl,--wrap=gst_value_set_caps
        -Wl,--wrap=gst_caps_merge
        -Wl,--wrap=gst_pad_set_event_function
        -Wl,--wrap=gst_pad_set_link_function
        -Wl,--wrap=gst_pad_set_unlink_function
        -Wl,--wrap=gst_pad_set_query_function
        -Wl,--wrap=gst_pad_get_eventfunc
        -Wl,--wrap=gst_pad_get_queryfunc
        -Wl,--wrap=g_object_new
        -Wl,--wrap=g_type_check_instance_cast
        -Wl,--wrap=g_type_check_instance_is_a
    )
else()
    target_link_options(L1_test_v4l2 PRIVATE --coverage
        -rdynamic
        -Wl,--allow-multiple-definition
        -Wl,--export-dynamic
        -Wl,--wrap=gst_base_sink_set_sync
        -Wl,--wrap=gst_base_sink_set_async_enabled
        -Wl,--wrap=gst_base_sink_is_async_enabled
        -Wl,--wrap=gst_buffer_peek_memory
        -Wl,--wrap=gst_element_class_set_static_metadata
        -Wl,--wrap=g_object_class_install_property
        -Wl,--wrap=g_object_class_override_property
        -Wl,--wrap=g_signal_new
        -Wl,--wrap=gst_caps_from_string
        -Wl,--wrap=gst_caps_new_simple
        -Wl,--wrap=gst_caps_unref
        -Wl,--wrap=gst_pad_template_new
        -Wl,--wrap=gst_static_pad_template_get
        -Wl,--wrap=gst_element_class_add_pad_template
        -Wl,--wrap=gst_type_find_factory_get_list
        -Wl,--wrap=gst_value_set_caps
        -Wl,--wrap=gst_caps_merge
        -Wl,--wrap=gst_pad_set_event_function
        -Wl,--wrap=gst_pad_set_link_function
        -Wl,--wrap=gst_pad_set_unlink_function
        -Wl,--wrap=gst_pad_set_query_function
        -Wl,--wrap=gst_pad_get_eventfunc
        -Wl,--wrap=gst_pad_get_queryfunc
        -Wl,--wrap=g_object_new
        -Wl,--wrap=g_type_check_instance_cast
        -Wl,--wrap=g_type_check_instance_is_a
    )
endif()

# Register test with CTest
add_test(NAME L1_test_v4l2 COMMAND L1_test_v4l2)