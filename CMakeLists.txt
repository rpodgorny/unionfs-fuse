project(unionfs-fuse)

add_definitions(-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26)

option(WITH_XATTR "Enable support for extended attributes")
if(WITH_XATTR)
	add_definitions(-DHAVE_SETXATTR)
endif(WITH_XATTR)

set(HASHTABLE_SRCS hashtable.c hashtable_itr.c hash.c elfhash.c)
set(UNIONFS_SRCS unionfs.c stats.c opts.c debug.c findbranch.c readdir.c general.c unlink.c cow.c cow_utils.c)

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})
target_link_libraries(unionfs fuse pthread m rt)
