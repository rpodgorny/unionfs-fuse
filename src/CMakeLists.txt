set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(UNIONFS_SRCS unionfs.c opts.c debug.c findbranch.c readdir.c
    general.c unlink.c cow.c cow_utils.c string.c rmdir.c usyslog.c
    fuse_ops.c)
set(UNIONFSCTL_SRCS unionfsctl.c)

SET(CMAKE_C_FLAGS "-pipe -W -Wall -DFORTIFY_SOURCE=2")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})

option(WITH_LIBFUSE3 "Enable libfuse3 support" ON)

find_package(PkgConfig REQUIRED)

IF (WITH_LIBFUSE3)
	add_definitions(-DFUSE_USE_VERSION=30)
	pkg_check_modules(FUSE REQUIRED fuse3)
ELSE (WITH_LIBFUSE3)
	add_definitions(-DFUSE_USE_VERSION=29)
	pkg_check_modules(FUSE REQUIRED fuse)
	target_link_libraries(unionfs pthread)
ENDIF (WITH_LIBFUSE3)

target_include_directories(unionfs PUBLIC ${FUSE_INCLUDE_DIRS})
target_compile_options(unionfs PUBLIC ${FUSE_CFLAGS_OTHER})
target_link_libraries(unionfs ${FUSE_LIBRARIES})

add_executable(unionfsctl ${UNIONFSCTL_SRCS})

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfs DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfsctl DESTINATION bin)
