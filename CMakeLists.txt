project(MARC-FS)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

# C++ standard
set(CMAKE_CXX_STANDARD 14)
MESSAGE(STATUS "Cmake system: " ${CMAKE_SYSTEM} )
MESSAGE(STATUS "Cmake arch: " ${CMAKE_SYSTEM_PROCESSOR} )

# custom compile flags
set(FUSE_REQUIRED_FLAGS "-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_REQUIRED_FLAGS}")

# modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules")

#                            #
# internal libs and packages #
#                            #

# Curl C++ bindings
ADD_SUBDIRECTORY(contrib/curlcpp)
INCLUDE_DIRECTORIES(${CURLCPP_SOURCE_DIR}/include)

# Googletest bindings
ADD_SUBDIRECTORY(contrib/googletest/googletest)
INCLUDE_DIRECTORIES(${gtest_SOURCE_DIR}/include)

#                            #
# external libs and packages #
#                            #

# FUSE
find_package(FUSE REQUIRED)
if(NOT FUSE_FOUND)
    message(SEND_ERROR "Failed to find fuse")
    return()
else()
    include_directories(${FUSE_INCLUDE_DIR})
endif()

# JSONCPP
find_package(JSONCPP REQUIRED)
if (NOT JSONCPP_FOUND)
    message(SEND_ERROR "Failed to find jsoncpp")
    return()
else()
    include_directories(${JSONCPP_INCLUDE_DIR})
endif()

# JEMALLOC - optional
find_package(JEMALLOC)
if(JEMALLOC_FOUND)
  include_directories(${JEMALLOC_INCLUDE_DIRS})
else()
    message(WARNING "jemalloc not found but highly recommended")
endif()

# first, run tests
add_executable(apitest tests/apitest.cpp)
add_library(marcfs_for_testing STATIC ${SRC_LIST})
target_link_libraries(apitest gtest_main marcfs_for_testing curlcpp jsoncpp)
add_test(NAME rest_api_test COMMAND apitest)

# final binary
add_executable(marcfs ${SRC_LIST})
target_link_libraries(marcfs
    curlcpp
    pthread
    ${JSONCPP_LIBRARIES}
    ${FUSE_LIBRARIES}
)

# optional libs
if(JEMALLOC_FOUND)
    target_link_libraries(marcfs ${JEMALLOC_LIBRARIES})
endif()
