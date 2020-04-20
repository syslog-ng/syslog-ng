add_subdirectory(csv-scanner)
add_subdirectory(list-scanner)
add_subdirectory(kv-scanner)
add_subdirectory(xml-scanner)

set(SCANNER_SOURCES
    scanner/${CSV_SCANNER_SOURCES}
    scanner/${KV_SCANNER_SOURCES}
    scanner/${LIST_SCANNER_SOURCES}
    scanner/${XML_SCANNER_SOURCES}
    PARENT_SCOPE)

set(SCANNER_HEADERS
    scanner/${CSV_SCANNER_HEADERS}
    scanner/${KV_SCANNER_HEADERS}
    scanner/${LIST_SCANNER_HEADERS}
    scanner/${XML_SCANNER_HEADERS}
    PARENT_SCOPE)
