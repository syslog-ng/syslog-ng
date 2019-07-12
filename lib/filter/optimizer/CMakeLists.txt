set(FILTER_OPTIMIZER_HEADERS
    filter/optimizer/filter-expr-optimizer.h
    filter/optimizer/filter-tree-printer.h
    filter/optimizer/concatenate-or-filters.h
    PARENT_SCOPE
)

set(FILTER_OPTIMIZER_SOURCES
    filter/optimizer/filter-tree-printer.c
    filter/optimizer/concatenate-or-filters.c
    PARENT_SCOPE
)

add_test_subdirectory(tests)
