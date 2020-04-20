set(TIMESTAMP_SOURCES
    timestamp-plugin.c
    timestamp-parser.c
    timestamp-parser.h
    date-parser.c
    date-parser.h
    rewrite-fix-timezone.c
    rewrite-fix-timezone.h
    rewrite-set-timezone.c
    rewrite-set-timezone.h
    rewrite-guess-timezone.c
    rewrite-guess-timezone.h
)

add_module(
  TARGET timestamp
  GRAMMAR timestamp-grammar
  SOURCES ${TIMESTAMP_SOURCES}
)

add_test_subdirectory(tests)
