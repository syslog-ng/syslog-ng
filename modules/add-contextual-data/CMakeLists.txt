set(add_contextual_data_SOURCES
    add-contextual-data.c
    add-contextual-data.h
    add-contextual-data-parser.c
    add-contextual-data-parser.h
    add-contextual-data-plugin.c
    context-info-db.h
    context-info-db.c
    contextual-data-record.h
    contextual-data-record.c
    contextual-data-record-scanner.h
    contextual-data-record-scanner.c
    add-contextual-data-selector.h
    add-contextual-data-template-selector.h
    add-contextual-data-template-selector.c
    add-contextual-data-filter-selector.h
    add-contextual-data-filter-selector.c
    add-contextual-data-glob-selector.h
    add-contextual-data-glob-selector.c
)

add_module(
  TARGET add_contextual_data
  GRAMMAR add-contextual-data-grammar
  SOURCES ${add_contextual_data_SOURCES}
)

add_test_subdirectory(tests)
