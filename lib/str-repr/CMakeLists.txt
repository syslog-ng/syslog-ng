set(STR_REPR_HEADERS
    str-repr/encode.h
    str-repr/decode.h
    PARENT_SCOPE)

set(STR_REPR_SOURCES
    str-repr/encode.c
    str-repr/decode.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
