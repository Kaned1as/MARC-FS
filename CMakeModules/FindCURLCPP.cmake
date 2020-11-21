# Find curlcpp
#
# Find the curlcpp includes and library
# 
# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH 
# 
# This module defines
#  CURLCPP_INCLUDE_DIRS, where to find header, etc.
#  CURLCPP_LIBRARIES, the libraries needed to use curlcpp.
#  CURLCPP_FOUND, If false, do not try to use curlcpp.
#  CURLCPP_INCLUDE_PREFIX, include prefix for curlcpp

# only look in default directories
find_path(
    CURLCPP_INCLUDE_DIR
    NAMES curl_header.h
    PATH_SUFFIXES curlcpp
    DOC "curlcpp include dir"
)

find_library(
    CURLCPP_LIBRARY
    NAMES curlcpp
    DOC "curlcpp library"
)

find_package(CURL REQUIRED)

set(CURLCPP_INCLUDE_DIRS ${CURLCPP_INCLUDE_DIR})
message(STATUS "curlcpp include dirs: " ${CURLCPP_INCLUDE_DIRS})
set(CURLCPP_LIBRARIES ${CURLCPP_LIBRARY} ${CURL_LIBRARIES})
message(STATUS "curlcpp libs: " ${CURLCPP_LIBRARIES})

# handle the QUIETLY and REQUIRED arguments and set CURLCPP_FOUND to TRUE
# if all listed variables are TRUE, hide their existence from configuration view
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(curlcpp DEFAULT_MSG CURLCPP_INCLUDE_DIR CURLCPP_LIBRARY)
mark_as_advanced (CURLCPP_INCLUDE_DIR CURLCPP_LIBRARY)
