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
    correlation.c
    correlation.h
    correlation-key.c
    correlation-key.h
    correlation-context.c
    correlation-context.h
    synthetic-message.c
    synthetic-message.h
    synthetic-context.c
    synthetic-context.h
    timerwheel.c
    timerwheel.h
    patternize.c
    patternize.h
    id-counter.h
    id-counter.c
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

set(CORRELATION_SOURCES
    stateful-parser.c
    stateful-parser.h
    dbparser.c
    dbparser.h
    correlation-parser.c
    correlation-parser.h
    correlation-plugin.c
    groupingby.c
    groupingby.h
    grouping-parser.c
    grouping-parser.h
    group-lines.c
    group-lines.h
)

add_module(
  TARGET correlation
  GRAMMAR correlation-grammar
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR} ${PATTERNDB_INCLUDE_DIR}
  DEPENDS patterndb
  SOURCES ${CORRELATION_SOURCES}
)

add_subdirectory(pdbtool)
add_test_subdirectory(tests)
