set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(UNIONFS_SRCS unionfs.c stats.c opts.c debug.c findbranch.c readdir.c 
    general.c unlink.c cow.c cow_utils.c string.c rmdir.c usyslog.c)

add_executable(unionfs ${UNIONFS_SRCS} ${HASHTABLE_SRCS})
target_link_libraries(unionfs fuse pthread)

if (UNIX AND NOT APPLE)
  target_link_libraries(rt)
endif()

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/unionfs DESTINATION bin)
