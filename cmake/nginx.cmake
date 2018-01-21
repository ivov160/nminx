cmake_minimum_required(VERSION 2.8)

find_package(Tar REQUIRED)

message(STATUS "downloading...
	src='https://nginx.ru/download/nginx-1.13.8.tar.gz'
	dst='${CMAKE_BINARY_DIR}/nginx-1.13.8.tar.gz'
	timeout='none'")

file(DOWNLOAD 
	"https://nginx.ru/download/nginx-1.13.8.tar.gz" "${CMAKE_BINARY_DIR}/nginx-1.13.8.tar.gz"
	STATUS nginx_download
	SHOW_PROGRESS)

message(STATUS "Unpack Nginx")
execute_process(
	COMMAND ${TAR_BIN} -xzf ${CMAKE_BINARY_DIR}/nginx-1.13.8.tar.gz 
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party)

message(STATUS "Configure Nginx headers")
execute_process(
	COMMAND ././configure
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/nginx-1.13.8)

