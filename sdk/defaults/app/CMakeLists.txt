cmake_minimum_required(VERSION 3.20)

# Before project(): the SDK selects the toolchain.
find_package(PebbleSDK REQUIRED HINTS $$ENV{PEBBLE_SDK})

project(${project_name} C)

pebble_app(SOURCES src/c/${project_name}.c)

if(EXISTS $${CMAKE_CURRENT_SOURCE_DIR}/worker_src)
  pebble_worker(SOURCES worker_src/c/${project_name}_worker.c)
endif()

if(EXISTS $${CMAKE_CURRENT_SOURCE_DIR}/src/pkjs)
  pebble_js(SOURCES src/pkjs/index.js)
endif()
