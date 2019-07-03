set(TIMESTAMP_SOURCES
    timestamp-plugin.c
    timestamp-parser.c
    timestamp-parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/timestamp-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/timestamp-grammar.h
    date-parser.c
    date-parser.h
    rewrite-fix-timezone.c
    rewrite-fix-timezone.h
    rewrite-set-timezone.c
    rewrite-set-timezone.h
    rewrite-guess-timezone.c
    rewrite-guess-timezone.h
)

generate_y_from_ym(modules/timestamp/timestamp-grammar)

bison_target(TimestampGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/timestamp-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/timestamp-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(timestamp SHARED ${TIMESTAMP_SOURCES})
target_include_directories(timestamp
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(timestamp PRIVATE syslog-ng)

install(TARGETS timestamp LIBRARY DESTINATION lib/syslog-ng/ COMPONENT timestamp)

add_test_subdirectory(tests)
