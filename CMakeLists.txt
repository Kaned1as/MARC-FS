project(MailFuse)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules")

find_package(FUSE REQUIRED)
if(NOT FUSE_FOUND)
    message(SEND_ERROR "Failed to find fuse")
    return()
else()
    include_directories(${FUSE_INCLUDE_DIR})
endif()

add_executable(${PROJECT_NAME} ${SRC_LIST})
