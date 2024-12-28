
# (Automatically converted from project Makefile by convert_to_cmake.py.)

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(targets "esp32" "esp32s2" "esp32s3" "esp32c3" "esp32c2" "esp32c6" "esp32c5" "esp32h2" "esp32p4")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wireless-gk)
