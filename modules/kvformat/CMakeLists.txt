set(KVFORMAT_SOURCES
    format-welf.c
    format-welf.h
    kv-parser.c
    kv-parser.h
    linux-audit-parser.c
    linux-audit-parser.h
    kv-parser-parser.c
    kv-parser-parser.h
    kvformat-plugin.c
    filterx-func-parse-kv.h
    filterx-func-parse-kv.c
    filterx-func-format-kv.h
    filterx-func-format-kv.c
)

add_module(
  TARGET kvformat
  GRAMMAR kv-parser-grammar
  SOURCES ${KVFORMAT_SOURCES}
)

add_test_subdirectory(tests)
