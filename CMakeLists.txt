cmake_minimum_required(VERSION 3.28)
project(encviz)

# Need C++17
set(CMAKE_CXX_STANDARD 17)

# Use ccache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()

# Required Libraries
include(FindPkgConfig)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(GDAL REQUIRED gdal)
pkg_check_modules(GTEST gtest_main gtest)
pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)
pkg_check_modules(TINYXML2 REQUIRED tinyxml2)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)

# Header locations
include_directories(
  ${CAIRO_INCLUDE_DIRS}
  ${GDAL_INCLUDE_DIRS}
  ${MICROHTTPD_INCLUDE_DIRS}
  ${TINYXML2_INCLUDE_DIRS}
  ${RSVG_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/include
  )

# Organize build products
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")

set(CMAKE_CXX_FLAGS "-O2 -Wall")

# Build source code
add_subdirectory(src)

# Build tests if able
if(GTEST_FOUND)
  enable_testing()
  add_subdirectory(test)
endif()
