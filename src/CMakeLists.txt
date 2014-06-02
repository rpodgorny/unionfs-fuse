set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(UNIONFS_SRCS unionfs.c stats.c opts.c debug.c findbranch.c readdir.c 
    general.c unlink.c cow.c cow_utils.c string.c rmdir.c usyslog.c)
set(UNIONFS_CTL_SRCS unionfs-fuse-ctl.c)

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})

if (UNIX AND NOT APPLE)
    target_link_libraries(unionfs fuse pthread rt)
else()
    target_link_libraries(unionfs fuse pthread)
endif()

add_executable(unionfs-ctl ${UNIONFS_CTL_SRCS})

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfs DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfs-ctl DESTINATION bin)
