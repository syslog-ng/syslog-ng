set(XML_SCANNER_HEADERS
    xml-scanner/xml-scanner.h
    PARENT_SCOPE)

set(XML_SCANNER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(XML_SCANNER_SOURCES
    xml-scanner/xml-scanner.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
