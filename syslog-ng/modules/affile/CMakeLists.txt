set(AFFILE_SOURCES
    "affile-dest.h"
    "affile-dest-internal-queue-filter.h"
    "affile-parser.h"
    "affile-source.h"
    "collection-comparator.h"
    "directory-monitor-factory.h"
    "directory-monitor.h"
    "directory-monitor-poll.h"
    "file-opener.h"
    "file-reader.h"
    "file-specializations.h"
    "logproto-file-reader.h"
    "logproto-file-writer.h"
    "named-pipe.h"
    "poll-file-changes.h"
    "poll-multiline-file-changes.h"
    "stdin.h"
    "transport-prockmsg.h"
    "wildcard-source.h"
    "wildcard-file-reader.h"
    "file-list.h"
    "affile-dest.c"
    "affile-parser.c"
    "affile-plugin.c"
    "affile-source.c"
    "collection-comparator.c"
    "directory-monitor.c"
    "directory-monitor-factory.c"
    "directory-monitor-poll.c"
    "file-opener.c"
    "file-reader.c"
    "linux-kmsg.c"
    "logproto-file-reader.c"
    "logproto-file-writer.c"
    "named-pipe.c"
    "poll-file-changes.c"
    "poll-multiline-file-changes.c"
    "regular-files.c"
    "stdin.c"
    "transport-prockmsg.c"
    "wildcard-source.c"
    "wildcard-file-reader.c"
    "file-list.c"
)

if(SYSLOG_NG_HAVE_INOTIFY)
  list(APPEND AFFILE_SOURCES
        "directory-monitor-inotify.h"
        "directory-monitor-inotify.c"
    )
endif()

add_module(
  TARGET affile
  GRAMMAR affile-grammar
  SOURCES ${AFFILE_SOURCES}
)

add_test_subdirectory(tests)
