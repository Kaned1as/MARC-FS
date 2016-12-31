project(MARC-FS)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

# C++ standard
set(CMAKE_CXX_STANDARD 14)

# custom compile flags
set(FUSE_REQUIRED_FLAGS "-D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_REQUIRED_FLAGS}")

# modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules")

# FUSE
find_package(FUSE REQUIRED)
if(NOT FUSE_FOUND)
    message(SEND_ERROR "Failed to find fuse")
    return()
else()
    include_directories(${FUSE_INCLUDE_DIR})
endif()

# Curl C++ bindings
ADD_SUBDIRECTORY(contrib/curlcpp)
INCLUDE_DIRECTORIES(${CURLCPP_SOURCE_DIR}/include)

# JSONCPP
find_package(JSONCPP REQUIRED)
if (NOT JSONCPP_FOUND)
    message(SEND_ERROR "Failed to find jsoncpp")
    return()
else()
    include_directories(${JSONCPP_INCLUDE_DIR})
endif()

add_executable("marcfs" ${SRC_LIST})
target_link_libraries("marcfs" curlcpp jsoncpp fuse pthread)
