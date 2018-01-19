flex_target(CfgLexer
    ${PROJECT_SOURCE_DIR}/lib/cfg-lex.l
    ${PROJECT_BINARY_DIR}/lib/cfg-lex.c
    ${PROJECT_BINARY_DIR}/lib/cfg-lex.h)

bison_target(CfgGrammar
    ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y
    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer CfgGrammar)

add_subdirectory(compat)
add_subdirectory(control)
add_subdirectory(debugger)
add_subdirectory(filter)
add_subdirectory(logmsg)
add_subdirectory(logproto)
add_subdirectory(parser)
add_subdirectory(rewrite)
add_subdirectory(stats)
add_subdirectory(template)
add_subdirectory(transport)
add_subdirectory(value-pairs)
add_subdirectory(scanner)
add_subdirectory(str-repr)
add_subdirectory(eventlog)

set(LIB_SUBDIR_HEADERS
    ${COMPAT_HEADERS}
    ${CONTROL_HEADERS}
    ${DEBUGGER_HEADERS}
    ${FILTER_HEADERS}
    ${LOGMSG_HEADERS}
    ${LOGPROTO_HEADERS}
    ${PARSER_HEADERS}
    ${REWRITE_HEADERS}
    ${STATS_HEADERS}
    ${TEMPLATE_HEADERS}
    ${TRANSPORT_HEADERS}
    ${VALUE_PAIRS_HEADERS}
    ${CSV_SCANNER_HEADERS}
    ${LIST_SCANNER_HEADERS}
    ${KV_SCANNER_HEADERS}
    ${STR_REPR_HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.h
)

set (LIB_HEADERS
    afinter.h
    alarms.h
    apphook.h
    atomic.h
    block-ref-parser.h
    cache.h
    cfg.h
    cfg-lexer.h
    cfg-lexer-subst.h
    cfg-args.h
    cfg-block.h
    cfg-block-generator.h
    cfg-parser.h
    cfg-tree.h
    children.h
    crypto.h
    dnscache.h
    driver.h
    fdhelpers.h
    file-perms.h
    find-crlf.h
    gprocess.h
    gsockaddr.h
    gsocket.h
    hostname.h
    host-resolve.h
    logmatcher.h
    logmpx.h
    logpipe.h
    logqueue-fifo.h
    logqueue.h
    logreader.h
    logsource.h
    logstamp.h
    logthrdestdrv.h
    logwriter.h
    mainloop.h
    mainloop-call.h
    mainloop-worker.h
    mainloop-io-worker.h
    module-config.h
    memtrace.h
    messages.h
    ml-batched-timer.h
    msg-format.h
    parse-number.h
    pathutils.h
    persist-state.h
    persistable-state-header.h
    persistable-state-presenter.h
    plugin.h
    plugin-types.h
    poll-events.h
    poll-fd-events.h
    pragma-parser.h
    presented-persistable-state.h
    reloc.h
    rcptid.h
    run-id.h
    scratch-buffers.h
    serialize.h
    service-management.h
    seqnum.h
    str-format.h
    str-utils.h
    syslog-names.h
    syslog-ng.h
    string-list.h
    timeutils.h
    tls-support.h
    thread-utils.h
    tlscontext.h
    type-hinting.h
    uuid.h
    userdb.h
    utf8utils.h
    versioning.h
    bookmark.h
    ringbuffer.h
    ack_tracker.h
    host-id.h
    resolved-configurable-paths.h
    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.h
    ${PROJECT_BINARY_DIR}/lib/block-ref-grammar.h
    ${PROJECT_BINARY_DIR}/lib/cfg-lex.h
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.h
    )

set(LIB_SOURCES
    afinter.c
    alarms.c
    apphook.c
    block-ref-parser.c
    cache.c
    cfg.c
    cfg-args.c
    cfg-block.c
    cfg-block-generator.c
    cfg-lexer.c
    cfg-lexer-subst.c
    cfg-parser.c
    cfg-tree.c
    children.c
    dnscache.c
    driver.c
    fdhelpers.c
    file-perms.c
    find-crlf.c
    globals.c
    gprocess.c
    gsockaddr.c
    gsocket.c
    hostname.c
    host-resolve.c
    logmatcher.c
    logmpx.c
    logpipe.c
    logqueue.c
    logqueue-fifo.c
    logreader.c
    logsource.c
    logstamp.c
    logthrdestdrv.c
    logwriter.c
    mainloop.c
    mainloop-call.c
    mainloop-worker.c
    mainloop-io-worker.c
    module-config.c
    memtrace.c
    messages.c
    ml-batched-timer.c
    msg-format.c
    parse-number.c
    pathutils.c
    persist-state.c
    plugin.c
    poll-events.c
    poll-fd-events.c
    pragma-parser.c
    persistable-state-presenter.c
    rcptid.c
    reloc.c
    run-id.c
    scratch-buffers.c
    serialize.c
    service-management.c
    str-format.c
    str-utils.c
    syslog-names.c
    string-list.c
    timeutils.c
    type-hinting.c
    ringbuffer.c
    late_ack_tracker.c
    early_ack_tracker.c
    crypto.c
    tlscontext.c
    uuid.c
    userdb.c
    utf8utils.c
    host-id.c
    resolved-configurable-paths.c
    ${COMPAT_SOURCES}
    ${CONTROL_SOURCES}
    ${DEBUGGER_SOURCES}
    ${FILTER_SOURCES}
    ${LOGMSG_SOURCES}
    ${LOGPROTO_SOURCES}
    ${PARSER_SOURCES}
    ${REWRITE_SOURCES}
    ${STATS_SOURCES}
    ${TEMPLATE_SOURCES}
    ${TRANSPORT_SOURCES}
    ${VALUE_PAIRS_SOURCES}
    ${SCANNER_SOURCES}
    ${STR_REPR_SOURCES}
    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.c
    ${PROJECT_BINARY_DIR}/lib/block-ref-grammar.c
    ${PROJECT_BINARY_DIR}/lib/cfg-lex.c
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.c
    )

generate_y_from_ym (lib/block-ref-grammar)
bison_target(BlockRefGrammar
    ${PROJECT_BINARY_DIR}/lib/block-ref-grammar.y
    ${PROJECT_BINARY_DIR}/lib/block-ref-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer BlockRefGrammar)

generate_y_from_ym (lib/pragma-grammar)
bison_target(PragmaGrammar
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.y
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer PragmaGrammar)

generate_y_from_ym (lib/filter/filter-expr-grammar)
bison_target(FilterExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer FilterExprGrammar)

generate_y_from_ym (lib/rewrite/rewrite-expr-grammar)
bison_target(RewriteExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer RewriteExprGrammar)

generate_y_from_ym (lib/parser/parser-expr-grammar)
bison_target(ParserExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer ParserExprGrammar)

set(Eventlog_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/eventlog/src/")

set(CORE_INCLUDE_DIRS
    ${Eventlog_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${Gettext_INCLUDE_DIR}
    ${IVYKIS_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBPCRE_INCLUDE_DIRS}
)

include_directories(SYSTEM ${CORE_INCLUDE_DIRS})

add_library(syslog-ng SHARED ${LIB_SOURCES})

target_include_directories(syslog-ng SYSTEM INTERFACE ${CORE_INCLUDE_DIRS})

target_link_libraries(
    syslog-ng
    LINK_PUBLIC
    eventlog
    ${BISON_LIBRARIES}
    ${Eventlog_LIBRARY}
    ${FLEX_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GTHREAD_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${Gettext_LIBRARIES}
    ${IVYKIS_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${RESOLV_LIBRARIES}
    ${LIBPCRE_LIBRARIES}
    )

set_target_properties(syslog-ng
    PROPERTIES VERSION ${SYSLOG_NG_VERSION}
    SOVERSION ${SYSLOG_NG_VERSION})

if     (${IVYKIS_INTERNAL})
   add_dependencies(syslog-ng IVYKIS)
endif()

install(TARGETS syslog-ng LIBRARY DESTINATION lib)

install(FILES ${LIB_HEADERS} DESTINATION include/syslog-ng)

install(FILES ${VALUE_PAIRS_HEADERS} DESTINATION include/syslog-ng/value-pairs)
install(FILES ${COMPAT_HEADERS} DESTINATION include/syslog-ng/compat)
install(FILES ${CONTROL_HEADERS} DESTINATION include/syslog-ng/control)
install(FILES ${DEBUGGER_HEADERS} DESTINATION include/syslog-ng/debugger)
install(FILES ${FILTER_HEADERS} DESTINATION include/syslog-ng/filter)
install(FILES ${LOGMSG_HEADERS} DESTINATION include/syslog-ng/logmsg)
install(FILES ${LOGPROTO_HEADERS} DESTINATION include/syslog-ng/logproto)
install(FILES ${PARSER_HEADERS} DESTINATION include/syslog-ng/parser)
install(FILES ${REWRITE_HEADERS} DESTINATION include/syslog-ng/rewrite)
install(FILES ${STATS_HEADERS} DESTINATION include/syslog-ng/stats)
install(FILES ${TEMPLATE_HEADERS} DESTINATION include/syslog-ng/template)
install(FILES ${TRANSPORT_HEADERS} DESTINATION include/syslog-ng/transport)
install(FILES ${CSV_SCANNER_HEADERS} DESTINATION include/syslog-ng/scanner/csv-scanner)

set(TOOLS
    merge-grammar.py
    cfg-grammar.y
)

install(FILES ${TOOLS} DESTINATION share/syslog-ng/tools)

add_subdirectory(tests)
