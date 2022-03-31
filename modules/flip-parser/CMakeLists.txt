set(FLIP_PARSER_SOURCES
    flip-parser.c
    flip-parser.h
    flip-parser-parser.c
    flip-parser-parser.h
    flip-parser-plugin.c
)

add_module(
  TARGET flip-parser
  GRAMMAR flip-parser-grammar
  SOURCES ${FLIP_PARSER_SOURCES}
)

