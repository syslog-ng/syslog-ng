set(AFFILE_HEADERS
    "affile-dest.h"
    "affile-dest-internal-queue-filter.h"
    "affile-parser.h"
    "affile-source.h"
    "collection-comporator.h"
    "directory-monitor-factory.h"
    "directory-monitor.h"
    "directory-monitor-inotify.h"
    "directory-monitor-poll.h"
    "file-opener.h"
    "file-reader.h"
    "file-specializations.h"
    "logproto-file-reader.h"
    "logproto-file-writer.h"
    "named-pipe.h"
    "poll-file-changes.h"
    "stdin.h"
    "transport-prockmsg.h"
    "wildcard-source.h"
    "${CMAKE_CURRENT_BINARY_DIR}/affile-grammar.h"
)

set(AFFILE_SOURCES
    "affile-dest.c"
    "affile-parser.c"
    "affile-plugin.c"
    "affile-source.c"
    "collection-comporator.c"
    "directory-monitor.c"
    "directory-monitor-factory.c"
    "directory-monitor-inotify.c"
    "directory-monitor-poll.c"
    "file-opener.c"
    "file-reader.c"
    "linux-kmsg.c"
    "logproto-file-reader.c"
    "logproto-file-writer.c"
    "named-pipe.c"
    "poll-file-changes.c"
    "regular-files.c"
    "stdin.c"
    "transport-prockmsg.c"
    "wildcard-source.c"
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
