cmake_minimum_required(VERSION 3.18)

find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-base-1.0
    gstreamer-video-1.0
)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

if(ENABLE_COVERAGE)
    find_program(LCOV_EXECUTABLE lcov REQUIRED)
    find_program(GENHTML_EXECUTABLE genhtml REQUIRED)
endif()

set(WESTEROS_SINK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(WESTEROS_SINK_MOCKS "${CMAKE_CURRENT_SOURCE_DIR}/mocks")

add_executable(test_westeros_sink_l1
    tests/westeros_sink_test.cpp
    mocks/sink_backend_mock.c
    mocks/wayland_protocol_mocks.c
    mocks/essos_resmgr_mock.c
    ${WESTEROS_SINK_ROOT}/westeros-sink.c
)

target_include_directories(test_westeros_sink_l1 BEFORE PRIVATE
    "${WESTEROS_SINK_MOCKS}"
    "${WESTEROS_SINK_ROOT}/raw"
    "${WESTEROS_SINK_ROOT}"
)

target_compile_definitions(test_westeros_sink_l1 PRIVATE
    UNIT_TEST
    USE_GST1
    USE_GST_VIDEO
    USE_GST_ALLOCATORS
    USE_RAW_SINK
    NO_GENERIC_AVSYNC
    VERSION="1.0.0"
    PACKAGE_NAME="westeros-sink"
)

target_compile_features(test_westeros_sink_l1 PRIVATE c_std_99 cxx_std_17)
target_compile_options(test_westeros_sink_l1 PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-deprecated-declarations>
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-but-set-variable>
)

target_link_libraries(test_westeros_sink_l1 PRIVATE
    GTest::gtest_main
    PkgConfig::GSTREAMER
    PkgConfig::WAYLAND_CLIENT
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

if(ENABLE_COVERAGE)
    # Only the production sink is measured; do not instrument GTest or mocks.
    set_source_files_properties("${WESTEROS_SINK_ROOT}/westeros-sink.c"
        PROPERTIES COMPILE_OPTIONS "-O0;-g;--coverage")
    target_link_options(test_westeros_sink_l1 PRIVATE --coverage)

    set(SINK_COVERAGE_OBJECT_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_westeros_sink_l1.dir")
    # Clear this target's counters, including stale counters from older builds
    # that instrumented the C++ tests. Never remove other targets' coverage.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reset_sink_coverage.cmake" [=[
file(GLOB_RECURSE counters "${COVERAGE_DIRECTORY}/*.gcda")
if(counters)
    file(REMOVE ${counters})
endif()
]=])

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/check_coverage_westeros_sink.cmake" [=[
if(NOT DEFINED COVERAGE_INFO OR NOT EXISTS "${COVERAGE_INFO}")
    message(FATAL_ERROR "Coverage tracefile not found: ${COVERAGE_INFO}")
endif()

file(STRINGS "${COVERAGE_INFO}" coverage_records REGEX "^(LF|LH):[0-9]+$")
set(total_found 0)
set(total_hit 0)
foreach(record IN LISTS coverage_records)
    if(record MATCHES "^LF:([0-9]+)$")
        set(count "${CMAKE_MATCH_1}")
        math(EXPR total_found "${total_found} + ${count}")
    elseif(record MATCHES "^LH:([0-9]+)$")
        set(count "${CMAKE_MATCH_1}")
        math(EXPR total_hit "${total_hit} + ${count}")
    endif()
endforeach()

if(total_found EQUAL 0)
    message(FATAL_ERROR "No executable lines were found in ${COVERAGE_INFO}")
endif()

math(EXPR coverage_percent "${total_hit} * 100 / ${total_found}")
message(STATUS "Westeros sink scoped line coverage: ${total_hit}/${total_found} (${coverage_percent}%)")
math(EXPR threshold_lhs "${total_hit} * 1000")
math(EXPR threshold_rhs "619 * ${total_found}")
if(threshold_lhs LESS threshold_rhs)
    message(FATAL_ERROR "Westeros sink scoped line coverage is below the required 61.9% (${total_hit}/${total_found})")
endif()
]=])

    add_custom_target(coverage-westeros-sink
        COMMAND "${CMAKE_COMMAND}"
            "-DCOVERAGE_DIRECTORY=${SINK_COVERAGE_OBJECT_DIR}"
            -P "${CMAKE_CURRENT_BINARY_DIR}/reset_sink_coverage.cmake"
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -L WesterosSink
        COMMAND "${LCOV_EXECUTABLE}" --capture
                --directory "${SINK_COVERAGE_OBJECT_DIR}"
                --include "*/westeros-sink.c"
            --output-file "${CMAKE_BINARY_DIR}/coverage_westeros_sink.info"
        COMMAND "${LCOV_EXECUTABLE}" --extract
                "${CMAKE_BINARY_DIR}/coverage_westeros_sink.info"
                "*/westeros-sink.c"
            --output-file "${CMAKE_BINARY_DIR}/coverage_westeros_sink_filtered.info"
        COMMAND "${CMAKE_COMMAND}"
            "-DCOVERAGE_INFO=${CMAKE_BINARY_DIR}/coverage_westeros_sink_filtered.info"
            -P "${CMAKE_CURRENT_BINARY_DIR}/check_coverage_westeros_sink.cmake"
        COMMAND "${GENHTML_EXECUTABLE}"
                "${CMAKE_BINARY_DIR}/coverage_westeros_sink_filtered.info"
                --output-directory "${CMAKE_BINARY_DIR}/coverage_html_westeros_sink"
            --title "Westeros Sink L1 Coverage" --legend --branch-coverage
        COMMAND "${LCOV_EXECUTABLE}" --summary
                "${CMAKE_BINARY_DIR}/coverage_westeros_sink_filtered.info"
        DEPENDS test_westeros_sink_l1
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Running Westeros sink L1 tests and reporting westeros-sink.c coverage"
        VERBATIM
    )
endif()

add_test(NAME WesterosSinkL1Tests COMMAND test_westeros_sink_l1)
set_tests_properties(WesterosSinkL1Tests PROPERTIES
    TIMEOUT 180
    LABELS "L1;WesterosSink;Component"
)