# - Find Tar
# Find the tar binnary
# This module defines
#  TAR_BIN, tar elf file.
#  TAR_FOUND, If false, do not try to use TAR_BIN.

set(TAR_FOUND FALSE)
set(TAR_BIN)

FIND_PROGRAM(TAR_BIN 
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

