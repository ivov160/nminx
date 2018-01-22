# - Find Git
# Find the git binnary
# This module defines
#  GIT_BIN, git elf file.
#  GIT_FOUND, If false, do not try to use GIT_BIN.

set(GIT_FOUND FALSE)
set(GIT_BIN)

FIND_PROGRAM(GIT_BIN 
	NAMES 
		git
	PATHS
		/bin
		/sbin
	PATH_SUFFIXES
		usr
	DOC "git bin"
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(git DEFAULT_MSG GIT_BIN)

IF(NOT GIT_FOUND)
	SET(GIT_BIN)
ENDIF ()

