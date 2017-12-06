add_subdirectory(csv-scanner)
add_subdirectory(list-scanner)
add_subdirectory(kv-scanner)

set(SCANNER_SOURCES
    scanner/${CSV_SCANNER_SOURCES}
    scanner/${KV_SCANNER_SOURCES}
    scanner/${LIST_SCANNER_SOURCES}
    PARENT_SCOPE)

set(SCANNER_HEADERS
    scanner/${CSV_SCANNER_HEADERS}
    scanner/${KV_SCANNER_HEADERS}
    scanner/${LIST_SCANNER_HEADERS}
    PARENT_SCOPE)
