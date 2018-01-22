cmake_minimum_required(VERSION 2.8)

macro(nginx_exists folder result)
	set(${result} FALSE)
	message(STATUS "Checking Nginx sources: ${folder}/nginx-${NGINX_VERSION}")
	if(EXISTS "${folder}/nginx-${NGINX_VERSION}")
		set(${result} TRUE)
	endif()
endmacro()

macro(nginx_download folder result)
	find_package(Tar REQUIRED)

	set(ngx_download_result_set FALSE)
	message(STATUS "Start download nginx-${NGINX_VERSION}")
	file(DOWNLOAD 
		"https://nginx.ru/download/nginx-${NGINX_VERSION}.tar.gz" "${CMAKE_BINARY_DIR}/nginx-${NGINX_VERSION}.tar.gz"
		STATUS ngx_download_result_set
		SHOW_PROGRESS)

	list(GET ngx_download_result_set 0 ngx_download_result)
	list(GET ngx_download_result_set 1 ngx_download_error)

	if(NOT ${ngx_download_result} EQUAL 0)
		message(FATAL_ERROR "Could not download nginx-${NGINX_VERSION}.tar.gz, error: ${ngx_download_error}")
	endif()

	message(STATUS "Extract nginx-${NGINX_VERSION} sources")
	execute_process(
		COMMAND ${TAR_BIN} -xzf "${CMAKE_BINARY_DIR}/nginx-${NGINX_VERSION}.tar.gz"
		WORKING_DIRECTORY "${folder}"
		RESULT_VARIABLE ngx_extract_code)

	if(NOT ${ngx_extract_code} EQUAL 0)
		message(FATAL_ERROR "Could not extract nginx-${NGINX_VERSION}.tar.gz")
	endif()
	set(${result} TRUE)
endmacro()

macro(nginx_configure folder result)
	message(STATUS "Configure Nginx headers")
	execute_process(
		COMMAND ./configure ${NGINX_FLAGS}
		WORKING_DIRECTORY ${folder}/nginx-${NGINX_VERSION}
		RESULT_VARIABLE ngx_configure_code
		OUTPUT_VARIABLE execute_ngx_configure_result)

	if(NOT ${ngx_configure_code} EQUAL 0)
		message(FATAL_ERROR "Could not configure nginx-${NGINX_VERSION}, flags: ${NGINX_FLAGS}")
	endif()
	set(${result} TRUE)
endmacro()

