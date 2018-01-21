# - Find netmap
# Find the netmap includes and library
# This module defines
#  TAR_BIN, where to find kernel module headers.
#  TAR_FOUND, If false, do not try to use netmap API.

FIND_program(TAR_BIN 
	NAMES 
		tar
	PATHS
		/bin
		/sbin
	PATH_SUFFIXES
		usr
	DOC "tar bin"
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(tar DEFAULT_MSG TAR_BIN)

IF(NOT TAR_FOUND)
	SET(TAR_BIN)
ENDIF ()

