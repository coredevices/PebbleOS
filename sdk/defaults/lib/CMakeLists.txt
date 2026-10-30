cmake_minimum_required(VERSION 3.20)

# Before project(): the SDK selects the toolchain.
find_package(PebbleSDK REQUIRED HINTS $$ENV{PEBBLE_SDK})

project(${project_name} C)

pebble_package(
  SOURCES src/c/${project_name}.c
  HEADERS include/${project_name}.h
)
target_include_directories(package INTERFACE include)

if(EXISTS $${CMAKE_CURRENT_SOURCE_DIR}/src/js)
  pebble_js(SOURCES src/js/index.js)
endif()
