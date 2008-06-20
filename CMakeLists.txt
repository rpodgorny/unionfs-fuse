project(unionfs-fuse)

# Set a default build type for single-configuration
# CMake generators if no build type is set.
IF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE RelWithDebInfo)
ENDIF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

# Select flags.
SET(CMAKE_C_FLAGS "-pipe -W -Wall -DFORTIFY_SOURCE=2")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")

add_definitions(-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26)

option(WITH_XATTR "Enable support for extended attributes")
if(WITH_XATTR)
	add_definitions(-DHAVE_SETXATTR)
endif(WITH_XATTR)

set(HASHTABLE_SRCS hashtable.c hashtable_itr.c hash.c elfhash.c)
set(UNIONFS_SRCS unionfs.c stats.c opts.c debug.c findbranch.c readdir.c general.c unlink.c cow.c cow_utils.c string.c rmdir.c)

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})
target_link_libraries(unionfs fuse pthread m rt)

INSTALL(PROGRAMS unionfs DESTINATION sbin)
INSTALL(FILES unionfs-fuse.8 DESTINATION share/man/man8/)

