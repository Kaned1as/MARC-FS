project(MARC-FS)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)

# determine, whether we want a static binary
set(STATIC_LINKING FALSE CACHE BOOL "Build a static binary?")
if(STATIC_LINKING)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    SET(CMAKE_EXE_LINKER_FLAGS "-static")
endif(STATIC_LINKING)

# C++ standard
set(CMAKE_CXX_STANDARD 14)
message(STATUS "Cmake system: " ${CMAKE_SYSTEM} )
message(STATUS "Cmake arch: " ${CMAKE_SYSTEM_PROCESSOR} )

# custom compile flags
set(FUSE_REQUIRED_FLAGS "-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=30")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_REQUIRED_FLAGS}")

# modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules")

#                            #
# internal libs and packages #
#                            #

# Curl C++ bindings
add_subdirectory(contrib/curlcpp EXCLUDE_FROM_ALL)
include_directories(${CURLCPP_SOURCE_DIR}/include)

# Googletest bindings
add_subdirectory(contrib/googletest/googletest EXCLUDE_FROM_ALL)
include_directories(${gtest_SOURCE_DIR}/include)

#                            #
# external libs and packages #
#                            #

# FUSE
find_package(FUSE3 REQUIRED)
if(NOT FUSE3_FOUND)
    message(SEND_ERROR "Failed to find fuse")
    return()
else()
    include_directories(${FUSE3_INCLUDE_DIR})
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
set(SRC_NO_MAIN ${SRC_LIST})
list(REMOVE_ITEM SRC_NO_MAIN ./main.cpp)
add_executable(apitest ${SRC_NO_MAIN} tests/apitest.cpp)
target_link_libraries(apitest
    gtest_main
    curlcpp
    ${JSONCPP_LIBRARIES}
    ${FUSE3_LIBRARIES})
add_test(NAME rest_api_test COMMAND apitest)

# final binary
add_executable(marcfs ${SRC_LIST})
target_link_libraries(marcfs
    curlcpp
    pthread
    ${JSONCPP_LIBRARIES}
    ${FUSE3_LIBRARIES}
)

# optional libs
if(JEMALLOC_FOUND)
    target_link_libraries(marcfs ${JEMALLOC_LIBRARIES})
endif()

install(TARGETS marcfs RUNTIME DESTINATION bin)
