# - Find Make
# Find the make binnary
# This module defines
#  MAKE_BIN, make elf file.
#  MAKE_FOUND, If false, do not try to use MAKE_BIN.

set(MAKE_FOUND FALSE)
set(MAKE_BIN)

FIND_program(MAKE_BIN 
	NAMES 
		make
	PATHS
		/bin
		/sbin
	PATH_SUFFIXES
		usr
	DOC "make bin"
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(make DEFAULT_MSG MAKE_BIN)

IF(NOT MAKE_FOUND)
	SET(MAKE_BIN)
ENDIF ()

