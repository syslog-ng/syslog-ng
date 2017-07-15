set(AFFILE_HEADERS
    "logproto-file-writer.h"
    "logproto-file-reader.h"
    "poll-file-changes.h"
    "transport-prockmsg.h"
    "file-reader.h"
    "file-opener.h"
    "file-specializations.h"
    "stdin.h"
    "named-pipe.h"
    "affile-source.h"
    "affile-dest.h"
    "affile-parser.h"
    "wildcard-source.h"
    "directory-monitor.h"
    "directory-monitor-inotify.h"
    "directory-monitor-poll.h"
    "directory-monitor-factory.h"
    "collection-comporator.h"
    "${CMAKE_CURRENT_BINARY_DIR}/affile-grammar.h"
)

set(AFFILE_SOURCES
    "logproto-file-writer.c"
    "logproto-file-reader.c"
    "poll-file-changes.c"
    "transport-prockmsg.c"
    "file-reader.c"
    "file-opener.c"
    "regular-files.c"
    "named-pipe.c"
    "linux-kmsg.c"
    "stdin.c"
    "affile-source.c"
    "affile-dest.c"
    "affile-parser.c"
    "affile-plugin.c"
    "wildcard-source.c"
    "directory-monitor.c"
    "directory-monitor-inotify.c"
    "directory-monitor-poll.c"
    "directory-monitor-factory.c"
    "collection-comporator.c"
    "${CMAKE_CURRENT_BINARY_DIR}/affile-grammar.c"
)

generate_y_from_ym(modules/affile/affile-grammar)

bison_target(AffileGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/affile-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/affile-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
add_library(affile MODULE ${AFFILE_SOURCES})
target_link_libraries(affile PRIVATE syslog-ng)

install(TARGETS affile
    LIBRARY DESTINATION lib/syslog-ng/
    COMPONENT affile)
