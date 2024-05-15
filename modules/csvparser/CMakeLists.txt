set(CSVPARSER_SOURCES
    csvparser.c
    csvparser.h
    csvparser-parser.c
    csvparser-parser.h
    csvparser-plugin.c
    filterx-func-parse-csv.h
    filterx-func-parse-csv.c
)

add_module(
  TARGET csvparser
  GRAMMAR csvparser-grammar
  SOURCES ${CSVPARSER_SOURCES}
)

add_test_subdirectory(tests)
