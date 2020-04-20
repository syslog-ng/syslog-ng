external_or_find_package(JSONC)

module_switch(ENABLE_JSON "Enable JSON plugin" JSONC_FOUND)
if (NOT ENABLE_JSON)
  return()
endif()

set(JSON_SOURCES
    format-json.c
    format-json.h
    json-parser.c
    json-parser.h
    json-parser-parser.c
    json-parser-parser.h
    dot-notation.c
    dot-notation.h
    json-plugin.c
)


add_module(
  TARGET json-plugin
  GRAMMAR json-parser-grammar
  INCLUDES ${JSONC_INCLUDE_DIR}
  DEPENDS ${JSONC_LIBRARY}
  SOURCES ${JSON_SOURCES}
)

if (JSONC_INTERNAL)
    add_dependencies(json-plugin JSONC)
endif()

add_test_subdirectory(tests)
