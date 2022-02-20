cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(MyQttSense_esp32)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "ca_certificates/broker.crt" TEXT)


## How do I change the build dir?
## None of these seem to work perfectly.
## the first one works provided that the directory exists
#1. set(BUILD_DIR "build_esp32")
#2. set(CMAKE_BINARY_DIR "build_esp32")
#3. idf_build_set_property(BUILD_DIR "build_esp32")
