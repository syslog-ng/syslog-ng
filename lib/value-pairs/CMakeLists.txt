set(VALUE_PAIRS_HEADERS
    value-pairs/value-pairs.h
    value-pairs/transforms.h
    value-pairs/cmdline.h
    value-pairs/evttag.h
    PARENT_SCOPE)

set(VALUE_PAIRS_SOURCES
    value-pairs/value-pairs.c
    value-pairs/transforms.c
    value-pairs/cmdline.c
    value-pairs/evttag.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
