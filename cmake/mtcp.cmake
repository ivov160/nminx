cmake_minimum_required(VERSION 2.8)

macro(mtcp_exists folder result)
	set(${result} FALSE)
	message(STATUS "Checking mTCP sources: ${folder}/mtcp")
	if(EXISTS "${folder}/mtcp")
		set(${result} TRUE)
	endif()
endmacro()

macro(mtcp_download folder result)
	find_package(Git REQUIRED)

	set(ngx_download_result_set FALSE)
	message(STATUS "Start download mtcp")
	execute_process(
		COMMAND ${GIT_BIN} clone https://github.com/eunyoung14/mtcp.git "${folder}/mtcp"
		RESULT_VARIABLE mtcp_download_code)

	if(NOT ${mtcp_download_code} EQUAL 0)
		message(FATAL_ERROR "Could not download mtcp sources")
	endif()

	#checkout for tag (mtcp not countains tag, only vershion hash)
	#another one solve is a git submodule
	#execute_process(
		#COMMAND ${GIT_BIN} checkout 59a6835e766bce2b874807997fdc24a4692d5c53
		#WORKING_DIRECTORY "${folder}/mtcp"
		#RESULT_VARIABLE mtcp_download_code)

	#if(NOT ${mtcp_download_code} EQUAL 0)
		#message(FATAL_ERROR "Could not change source tag")
	#endif()

	set(${result} TRUE)
endmacro()

macro(mtcp_configure folder result)
	message(STATUS "Configure mTCP with cflags: ${MTCP_CFLAGS} flags: ${MTCP_FLAGS}")

	set(backup_cflags $ENV{CFLAGS})
	set(ENV{CFLAGS} ${MTCP_CFLAGS})
	execute_process(
		COMMAND ./configure ${MTCP_FLAGS}
		WORKING_DIRECTORY ${folder}/mtcp
		RESULT_VARIABLE mtcp_configure_code)
		#OUTPUT_VARIABLE execute_mtcp_configure_stdout
		#ERROR_VARIABLE execute_mtcp_configure_stderr)
	set(ENV{CFLAGS} ${backup_cflags})

	if(NOT ${ngx_configure_code} EQUAL 0)
		message(FATAL_ERROR "Could not configure mTCP with flags: ${MTCP_FLAGS}")
	endif()
	set(${result} TRUE)
endmacro()

macro(mtcp_build folder result)
	find_package(Make REQUIRED)

	message(STATUS "Build mTCP with cflags: ${MTCP_CFLAGS} flags: ${MTCP_FLAGS}")
	
	# build only lib (mtcp pice of shit)
	# ${folder}/mtcp/mtcp/src
	execute_process(
		COMMAND ${MAKE_BIN} -C ${folder}/mtcp/mtcp/src
		WORKING_DIRECTORY ${folder}/mtcp
		RESULT_VARIABLE mtcp_build_code)
		#OUTPUT_VARIABLE execute_mtcp_configure_stdout
		#ERROR_VARIABLE execute_mtcp_configure_stderr)

	if(NOT ${mtcp_build_code} EQUAL 0)
		message(FATAL_ERROR "Could not build mTCP library")
	endif()
	set(${result} TRUE)
endmacro()

