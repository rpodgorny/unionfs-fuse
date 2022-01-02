set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(UNIONFS_SRCS unionfs.c opts.c debug.c findbranch.c readdir.c
    general.c unlink.c cow.c cow_utils.c string.c rmdir.c usyslog.c
    fuse_ops.c)
set(UNIONFSCTL_SRCS unionfsctl.c)

SET(CMAKE_C_FLAGS "-pipe -W -Wall -DFORTIFY_SOURCE=2")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")

# this should not be needed anymore since we use pkg_check_modules (see below)
#if (UNIX AND APPLE)
#	# Legacy include path for osxfuse
#	include_directories("/usr/local/include/osxfuse/fuse")
#	# New include path for MacFuse
#	include_directories("/usr/local/include/fuse")
#	link_directories("/usr/local/lib")
#endif()

add_definitions(-DFUSE_USE_VERSION=30)

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse)
target_include_directories(unionfs PUBLIC ${FUSE_INCLUDE_DIRS})
target_compile_options(unionfs PUBLIC ${FUSE_CFLAGS_OTHER})
target_link_libraries(unionfs ${FUSE_LIBRARIES} pthread)

add_executable(unionfsctl ${UNIONFSCTL_SRCS})

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfs DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfsctl DESTINATION bin)
