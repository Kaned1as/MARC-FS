project(MARC-FS)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(src SRC_LIST)

# Options
option(USE_JEMALLOC "Whether to use jemalloc" FALSE)
option(STATIC_LINKING "Build a static binary" FALSE)

# determine, whether we want a static binary
if(STATIC_LINKING)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "-static -Bstatic")
    set(CMAKE_LINK_SEARCH_START_STATIC 1)
    set(CMAKE_LINK_SEARCH_END_STATIC 1)
endif(STATIC_LINKING)

# C++ standard
set(CMAKE_CXX_STANDARD 17)

# custom compile flags
set(FUSE_REQUIRED_FLAGS "-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=35")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_REQUIRED_FLAGS}")

# modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules")

#                            #
# internal libs and packages #
#                            #


# Curl C++ bindings
add_subdirectory(contrib/curlcpp EXCLUDE_FROM_ALL)
include_directories(${CURLCPP_SOURCE_DIR}/include)
list(APPEND REQUIRED_LIBS curlcpp)
list(APPEND TEST_LIBS curlcpp)

# Googletest bindings
add_subdirectory(contrib/googletest/googletest EXCLUDE_FROM_ALL)
include_directories(${gtest_SOURCE_DIR}/include)
list(APPEND TEST_LIBS gtest_main)

#                            #
# external libs and packages #
#                            #

# FUSE
find_package(FUSE3 REQUIRED)
include_directories(${FUSE3_INCLUDE_DIR})
list(APPEND REQUIRED_LIBS ${FUSE3_LIBRARIES})
list(APPEND TEST_LIBS ${FUSE3_LIBRARIES})

# JSONCPP
find_package(JSONCPP REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIR})
list(APPEND REQUIRED_LIBS ${JSONCPP_LIBRARIES})
list(APPEND TEST_LIBS ${JSONCPP_LIBRARIES})

list(APPEND REQUIRED_LIBS pthread)
list(APPEND TEST_LIBS pthread)

if(STATIC_LINKING)
    # find libcurl flags through pkg-config
    find_package(PkgConfig QUIET)
    pkg_check_modules(CURL libcurl REQUIRED)
    list(APPEND REQUIRED_LIBS ${CURL_STATIC_LIBRARIES})
    list(APPEND TEST_LIBS ${CURL_STATIC_LIBRARIES})
endif()

# JEMALLOC - optional
if(USE_JEMALLOC)
    find_package(JEMALLOC)
    if(JEMALLOC_FOUND)
        include_directories(${JEMALLOC_INCLUDE_DIRS})
    else()
        message(WARNING "jemalloc not found but highly recommended")
    endif()
endif(USE_JEMALLOC)

# first, run tests
set(SRC_NO_MAIN ${SRC_LIST})
list(REMOVE_ITEM SRC_NO_MAIN src/main.cpp)
add_executable(apitest ${SRC_NO_MAIN} tests/apitest.cpp)
target_link_libraries(apitest ${TEST_LIBS})
add_test(NAME rest_api_test COMMAND apitest)

# final binary
add_executable(marcfs ${SRC_LIST})
target_link_libraries(marcfs ${REQUIRED_LIBS})

# optional libs
if(USE_JEMALLOC)
    if(JEMALLOC_FOUND)
        target_link_libraries(marcfs ${JEMALLOC_LIBRARIES})
    endif()
endif(USE_JEMALLOC)

install(TARGETS marcfs RUNTIME DESTINATION bin)
