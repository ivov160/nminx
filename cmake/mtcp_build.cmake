cmake_minimum_required(VERSION 2.8)

#message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
#message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
include(mtcp)


mtcp_build(${THIRD_PARTY_PATH} MTCP_BUILD_RESULT)
if(NOT ${MTCP_BUILD_RESULT})
	message(FATAL_ERROR "Could not build mtcp")
endif()
