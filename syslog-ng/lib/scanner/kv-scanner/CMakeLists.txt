set(KV_SCANNER_HEADERS
    kv-scanner/kv-scanner.h
    PARENT_SCOPE)

set(KV_SCANNER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(KV_SCANNER_SOURCES
    kv-scanner/kv-scanner.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
