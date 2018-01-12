cmake_minimum_required(VERSION 2.8)

# Project
set(target nmninx)
project(${target})

# Version number
set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

#added cmake folder to cmake modules search
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Settings
#include(settings)

find_package(netmap REQUIRED)
find_package(mTCP REQUIRED)

# Add header location
include_directories(
	/usr/lib
	/usr/local/lib
	${CMAKE_SOURCE_DIR}/include
)

file(GLOB_RECURSE app_src RELATIVE ${CMAKE_SOURCE_DIR} "src/*")

## Source files
add_executable(${PROJECT_NAME} ${app_src})
target_link_libraries(${PROJECT_NAME} mtcp)

