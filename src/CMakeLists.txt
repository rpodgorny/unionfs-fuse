set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(UNIONFS_SRCS unionfs.c opts.c debug.c findbranch.c readdir.c
    general.c unlink.c cow.c cow_utils.c string.c rmdir.c usyslog.c
    fuse_ops.c)
set(UNIONFSCTL_SRCS unionfsctl.c)

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})

if (UNIX AND NOT APPLE)
    target_link_libraries(unionfs fuse3 pthread rt)
else()
    target_link_libraries(unionfs fuse3 pthread)
endif()

add_executable(unionfsctl ${UNIONFSCTL_SRCS})

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfs DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfsctl DESTINATION bin)
