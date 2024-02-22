# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
IF(POLICY CMP0012)
    CMAKE_POLICY(SET CMP0012 NEW)
ENDIF()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(FireBeetleInternalEspIdf)

idf_build_unset_property(CXX_COMPILE_OPTIONS "-std=gnu++11")
