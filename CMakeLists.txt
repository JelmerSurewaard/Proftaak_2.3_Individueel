# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(HTTP_ONLY ON CACHE BOOL "")
set(CMAKE_USE_OPENSSL OFF CACHE BOOL "")
set(BUILD_CURL_EXE OFF CACHE BOOL "")
set(CURL_STATICLIB ON CACHE BOOL "")
set(CURL_ZLIB OFF CACHE BOOL "")
set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "")

project(living_stream_m3u_aac)