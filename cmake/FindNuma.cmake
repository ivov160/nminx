# - Find Numa
# Find the LibNuma includes and library
# This module defines
#  NUMA_INCLUDE_DIR, where to find library headers.
#  NUMA_FOUND, If false, do not try to use mTCP.
#  also defined, but not for general use are
#  NUMA_LIBRARY, where to find the mTCP library.

FIND_PATH(NUMA_INCLUDE_DIR 
	NAMES 
		numa.h
	PATHS
		/usr/local/include
		/usr/include
	DOC "Numa include directory path"
)

FIND_LIBRARY(NUMA_LIBRARY
    NAMES 
		numa	
    PATHS 
		/usr/lib 
		/usr/local/lib
	DOC "Numa library"
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Numa DEFAULT_MSG NUMA_INCLUDE_DIR NUMA_LIBRARY)

IF(NOT NUMA_FOUND)
	SET(NUMA_INCLUDE_DIR)
	SET(NUMA_LIBRARY)
ENDIF ()

