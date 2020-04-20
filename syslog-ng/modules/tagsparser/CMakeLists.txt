set(TAGPARSER_SOURCES
    tags-parser.c
    tags-parser.h
    tags-parser-parser.c
    tags-parser-parser.h
    tags-parser-plugin.c
)

add_module(
  TARGET tags-parser
  GRAMMAR tags-parser-grammar
  SOURCES ${TAGPARSER_SOURCES}
)

add_test_subdirectory(tests)
