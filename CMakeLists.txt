cmake_minimum_required(VERSION 2.8)

# Project
set(target nminx)
project(${target})

# Version number
set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

option(BUILD_DOCS	"Build doc" OFF) 
option(SYSTEM_MTCP	"Use system mTcp" OFF)
option(MTCP_FLAGS	"mTCP flags for build" OFF)
option(MTCP_CFLAGS	"mTCP cflags for build" OFF)

set(PROJECT_DOCS_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/doc")

# added cmake folder to cmake modules search
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# output dirrectories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Settings
include(nginx)

if(BUILD_DOCS)
	find_package(Doxygen)
	if(${DOXYGEN_FOUND})
		message(STATUS "Doxygen found")

		set(PROJECT_DOC_DIRS
			" ${CMAKE_SOURCE_DIR}/README.md \\
			${CMAKE_SOURCE_DIR}/src \\
			${CMAKE_SOURCE_DIR}/include/ \\
			${CMAKE_SOURCE_DIR}/doxfiles \\
		")

		configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile)
		add_custom_target(docs 
			${CMAKE_COMMAND} -E make_directory ${PROJECT_DOCS_OUTPUT_DIRECTORY}
			COMMAND doxygen 2>doxygen.log
			COMMAND test -f ./doxygen.log && echo "Error occurred while doxygen compiling documentation. Check doxygen.log"
		)
	else()
		message(STATUS "Doxygen not found, BUILD_DOCS ignored")
	endif()
endif()

set(THIRD_PARTY_PATH "${CMAKE_SOURCE_DIR}/third_party")
set(NGINX_VERSION "1.13.8")
set(NGINX_FLAGS "")
set(NGINX_PATH "${THIRD_PARTY_PATH}/nginx-${NGINX_VERSION}")

nginx_exists(${THIRD_PARTY_PATH} NGINX_SRC_FOUND)
if(NOT ${NGINX_SRC_FOUND})
	message(STATUS "Not found Nginx sources version of: ${NGINX_VERSION}")
	nginx_download(${THIRD_PARTY_PATH} NGINX_DOWNLOAD_RESULT)

	if(NOT NGINX_DOWNLOAD_RESULT)
		message(FATAL_ERROR "Could not download nginx sources of version: ${NGINX_VERSION}")
	endif()

	nginx_configure(${THIRD_PARTY_PATH} NGINX_CONFIGURE_RESULT)
	if(NOT ${NGINX_CONFIGURE_RESULT})
		message(FATAL_ERROR "Could not configure nginx with flags: ${NGINX_FLAGS}")
	endif()
endif()

#checking deps
find_package(netmap REQUIRED)
find_package(Numa REQUIRED)

if(NOT ${SYSTEM_MTCP})
	if(NOT ${MTCP_FLAGS})
		set(MTCP_FLAGS "--enable-netmap")
	endif()

	if(NOT ${MTCP_CFLAGS})
		set(MTCP_CFLAGS "${CMAKE_C_FLAGS}")
	endif()

	include(mtcp)

	mtcp_exists(${THIRD_PARTY_PATH} MTCP_SRC_FOUND)
	if(NOT ${MTCP_SRC_FOUND})
		message(STATUS "Not found mtcp sources")
		mtcp_download(${THIRD_PARTY_PATH} MTCP_DOWNLOAD_RESULT)

		if(NOT MTCP_DOWNLOAD_RESULT)
			message(FATAL_ERROR "Could not download mtcp sources")
		endif()

		mtcp_configure(${THIRD_PARTY_PATH} MTCP_CONFIGURE_RESULT)
		if(NOT ${MTCP_CONFIGURE_RESULT})
			message(FATAL_ERROR "Could not configure mtcp with cflags: ${MTCP_CFLAGS} flags: ${MTCP_FLAGS}")
		endif()
	endif()

	add_custom_target(mtcp_build ALL
		COMMAND
		${CMAKE_COMMAND} 
		-DMTCP_FLAGS=${MTCP_FLAGS}
		-DMTCP_CFLAGS=${MTCP_CFLAGS}
		-DTHIRD_PARTY_PATH=${THIRD_PARTY_PATH}
		-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}
		-P ${CMAKE_SOURCE_DIR}/cmake/mtcp_build.cmake)

	set(CMAKE_INCLUDE_PATH "${THIRD_PARTY_PATH}/mtcp/mtcp/include")
	set(CMAKE_LIBRARY_PATH "${THIRD_PARTY_PATH}/mtcp/mtcp/lib")
endif()
find_package(mTCP REQUIRED)

# Add header location
include_directories(
	/usr/lib
	/usr/local/lib
	${MTCP_INCLUDE_DIR}
	${NUMA_INCLUDE_DIR}
	${CMAKE_SOURCE_DIR}/include
	${NGINX_PATH}/src/os/unix
	${NGINX_PATH}/objs
)

file(GLOB_RECURSE app_src RELATIVE ${CMAKE_SOURCE_DIR} "src/*")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pthread ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

## Source files
add_executable(${PROJECT_NAME} ${app_src})
target_link_libraries(${PROJECT_NAME} ${MTCP_LIBRARY} ${NUMA_LIBRARY})

if(NOT ${SYSTEM_MTCP})
	add_dependencies(${PROJECT_NAME} mtcp_build)
endif()


# Strip files
if(NOT ${CMAKE_BUILD_TYPE} MATCHES "DEB")
    add_custom_target(strip)
    add_custom_command(TARGET strip
                       POST_BUILD
                       COMMAND strip ${target}
                       WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endif()
