set(LOGMSG_HEADERS
    logmsg/gsockaddr-serialize.h
    logmsg/logmsg.h
    logmsg/logmsg-serialize.h
    logmsg/logmsg-serialize-fixup.h
    logmsg/nvhandle-descriptors.h
    logmsg/nvtable.h
    logmsg/nvtable-serialize.h
    logmsg/nvtable-serialize-endianutils.h
    logmsg/tags-serialize.h
    logmsg/timestamp-serialize.h
    logmsg/tags.h
    PARENT_SCOPE)

set(LOGMSG_SOURCES
    logmsg/gsockaddr-serialize.c
    logmsg/logmsg.c
    logmsg/logmsg-serialize.c
    logmsg/logmsg-serialize-fixup.c
    logmsg/nvtable.c
    logmsg/nvtable-serialize.c
    logmsg/tags-serialize.c
    logmsg/timestamp-serialize.c
    logmsg/tags.c
    PARENT_SCOPE)

add_subdirectory(tests)
