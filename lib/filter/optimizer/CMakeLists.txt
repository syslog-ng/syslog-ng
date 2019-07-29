set(FILTER_OPTIMIZER_HEADERS
    filter/optimizer/filter-expr-optimizer.h
    filter/optimizer/filter-tree-printer.h
    PARENT_SCOPE
)

set(FILTER_OPTIMIZER_SOURCES
    filter/optimizer/filter-tree-printer.c
    filter/optimizer/filter-expr-optimizer-registry.c
    PARENT_SCOPE
)

add_test_subdirectory(tests)
