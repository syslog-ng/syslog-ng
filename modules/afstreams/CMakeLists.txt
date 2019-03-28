set(AFSTREAMS_SOURCES
    afstreams.c
    afstreams.h
    afstreams-parser.c
    afstreams-parser.h
    afstreams-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/afstreams-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/afstreams-grammar.c
)

include (CheckIncludeFile)
check_include_file(sys/strlog.h HAVE_STRLOG)
check_include_file(stropts.h HAVE_STROPTS)

module_switch(ENABLE_SUN_STREAMS "Enable Sun Streams source" HAVE_STRLOG HAVE_STROPTS)
if (ENABLE_SUN_STREAMS)
    generate_y_from_ym(modules/afstreams/afstreams-grammar)

    bison_target(StreamsGrammar
        ${CMAKE_CURRENT_BINARY_DIR}/afstreams-grammar.y
        ${CMAKE_CURRENT_BINARY_DIR}/afstreams-grammar.c
        COMPILE_FLAGS ${BISON_FLAGS})

    add_library(afstreams MODULE ${AFSTREAMS_SOURCES})
    target_include_directories (afstreams PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(afstreams PRIVATE syslog-ng)

    install(TARGETS afstreams LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afstreams)
endif()
