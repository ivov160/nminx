# - Find netmap
# Find the netmap includes and library
# This module defines
#  NETMAP_INCLUDE_DIR, where to find kernel module headers.
#  NETMAP_FOUND, If false, do not try to use netmap API.

set(NETMAP_FOUND FALSE)
set(NETMAP_LIBRARY)
set(NETMAP_INCLUDE_DIR)

FIND_FILE(NETMAP_INCLUDE_DIR 
	NAMES
		netmap.h
		netmap_user.h
		netmap_virt.h
	PATHS
		/usr/include
		/usr/local/include
		/usr/local/include/net
	DOC "netmap include directory path"
)

#modinfo netmap
#FIND_LIBRARY(MTCP_LIBRARY
    #NAMES 
		#libmtcp.a
    #PATHS 
		#/usr/lib 
		#/usr/local/lib
	#DOC "netmap kernel module path"
#)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(netmap DEFAULT_MSG NETMAP_INCLUDE_DIR)

IF(NOT NETMAP_FOUND)
	SET(NETMAP_INCLUDE_DIR)
ENDIF ()

