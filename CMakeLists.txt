cmake_minimum_required(VERSION 3.18)
project(westeros_sink_l1 LANGUAGES C CXX)

option(ENABLE_COVERAGE "Instrument the production sink source for L1 coverage" OFF)

enable_testing()
add_subdirectory(L1)
