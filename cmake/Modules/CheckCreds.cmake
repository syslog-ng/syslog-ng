include (CheckTypeSize)

set (CMAKE_EXTRA_INCLUDE_FILES sys/types.h sys/socket.h)
set (CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE=1)
check_type_size ("struct ucred" STRUCT_UCRED)
check_type_size ("struct cmsgcred" STRUCT_CMSGCRED)
unset (CMAKE_EXTRA_INCLUDE_FILES)
unset (CMAKE_REQUIRED_DEFINITIONS)
