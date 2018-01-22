# - Find mTCP
# Find the mTCP includes and library
# This module defines
#  MTCP_INCLUDE_DIR, where to find library headers.
#  MTCP_FOUND, If false, do not try to use mTCP.
#  also defined, but not for general use are
#  MTCP_LIBRARY, where to find the mTCP library.

set(MTCP_FOUND FALSE)
set(MTCP_LIBRARY)
set(MTCP_INCLUDE_DIR)

FIND_PATH(MTCP_INCLUDE_DIR 
	NAMES 
		mtcp_api.h 
		mtcp_epoll.h
	PATHS
		/usr/include
		/usr/local/include
	DOC "mTCP include directory path"
)
message("HZ: ${MTCP_INCLUDE_DIR}")

FIND_LIBRARY(MTCP_LIBRARY
    NAMES 
		libmtcp.a
    PATHS 
		/usr/lib 
		/usr/local/lib
	DOC "mTCP library"
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(mTCP DEFAULT_MSG MTCP_INCLUDE_DIR MTCP_LIBRARY)

IF(NOT MTCP_FOUND)
	SET(MTCP_INCLUDE_DIR)
	SET(MTCP_LIBRARY)
ENDIF ()

