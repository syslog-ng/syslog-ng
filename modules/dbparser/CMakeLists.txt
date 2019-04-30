set(PATTERNDB_SOURCES
    radix.c
    radix.h
    patterndb.c
    patterndb.h
    pdb-load.c
    pdb-load.h
    pdb-rule.c
    pdb-rule.h
    pdb-file.c
    pdb-file.h
    pdb-error.c
    pdb-error.h
    pdb-action.c
    pdb-action.h
    pdb-program.c
    pdb-program.h
    pdb-example.c
    pdb-example.h
    pdb-ruleset.c
    pdb-ruleset.h
    pdb-context.c
    pdb-context.h
    pdb-ratelimit.c
    pdb-ratelimit.h
    pdb-lookup-params.h
    pdb-lookup-params.c
    correllation.c
    correllation.h
    correllation-key.c
    correllation-key.h
    correllation-context.c
    correllation-context.h
    synthetic-message.c
    synthetic-message.h
    synthetic-context.c
    synthetic-context.h
    timerwheel.c
    timerwheel.h
    patternize.c
    patternize.h
)

set(PATTERNDB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(patterndb ${PATTERNDB_SOURCES})
# The radix tree uses unsigned char internally but passes these values to
# string.h functions, which trigger a lot of harmless warnings.  Of course
# these could be fixed by adding a lot of casts to the code, but I
# considered these to be less readable, than simply adding -Wno-pointer-sign
# warning option.
#
set_target_properties(patterndb PROPERTIES COMPILE_FLAGS "-fPIC -Wno-pointer-sign")
target_include_directories(patterndb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(patterndb PUBLIC syslog-ng)

set(DBPARSER_SOURCES
    stateful-parser.c
    stateful-parser.h
    dbparser.c
    dbparser.h
    dbparser-parser.c
    dbparser-parser.h
    dbparser-plugin.c
    groupingby.c
    groupingby.h
    ${CMAKE_CURRENT_BINARY_DIR}/dbparser-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/dbparser-grammar.h
)

generate_y_from_ym(modules/dbparser/dbparser-grammar)

bison_target(DBParserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/dbparser-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/dbparser-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(dbparser SHARED ${DBPARSER_SOURCES})
target_link_libraries(dbparser PRIVATE patterndb)
target_include_directories(dbparser
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(dbparser PRIVATE syslog-ng)

install(TARGETS dbparser
        LIBRARY DESTINATION lib/syslog-ng/
        COMPONENT dbparser)
add_subdirectory(pdbtool)
add_test_subdirectory(tests)
