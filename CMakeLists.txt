project(MailFuse)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

# C++ standard
set (CMAKE_CXX_STANDARD 14)

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

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} curlcpp jsoncpp)
