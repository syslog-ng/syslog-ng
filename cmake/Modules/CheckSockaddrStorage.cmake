include (CheckTypeSize)

set (CMAKE_EXTRA_INCLUDE_FILES sys/socket.h)
check_type_size ("struct sockaddr_storage" STRUCT_SOCKADDR_STORAGE)
set (CMAKE_EXTRA_INCLUDE_FILES)
