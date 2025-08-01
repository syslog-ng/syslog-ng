flex_target(CfgLexer
    ${PROJECT_SOURCE_DIR}/lib/cfg-lex.l
    ${PROJECT_BINARY_DIR}/lib/cfg-lex.c
    ${PROJECT_BINARY_DIR}/lib/cfg-lex.h)

bison_target(CfgGrammar
    ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y
    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS})
add_flex_bison_dependency(CfgLexer CfgGrammar)

add_subdirectory(ack-tracker)
add_subdirectory(compat)
add_subdirectory(control)
add_subdirectory(healthcheck)
add_subdirectory(debugger)
add_subdirectory(filter)
add_subdirectory(logmsg)
add_subdirectory(logproto)
add_subdirectory(multi-line)
add_subdirectory(parser)
add_subdirectory(rewrite)
add_subdirectory(stats)
add_subdirectory(metrics)
add_subdirectory(template)
add_subdirectory(transport)
add_subdirectory(value-pairs)
add_subdirectory(scanner)
add_subdirectory(str-repr)
add_subdirectory(timeutils)
add_subdirectory(eventlog)
add_subdirectory(secret-storage)
add_subdirectory(logthrsource)
add_subdirectory(logthrdest)
add_subdirectory(signal-slot-connector)

set(LIB_SUBDIR_HEADERS
    ${ACK_TRACKER_HEADERS}
    ${COMPAT_HEADERS}
    ${CONTROL_HEADERS}
    ${HEALTHCHECK_HEADERS}
    ${DEBUGGER_HEADERS}
    ${FILTER_HEADERS}
    ${LOGMSG_HEADERS}
    ${LOGPROTO_HEADERS}
    ${MULTI_LINE_HEADERS}
    ${PARSER_HEADERS}
    ${REWRITE_HEADERS}
    ${STATS_HEADERS}
    ${METRICS_HEADERS}
    ${TEMPLATE_HEADERS}
    ${TRANSPORT_HEADERS}
    ${VALUE_PAIRS_HEADERS}
    ${CSV_SCANNER_HEADERS}
    ${LIST_SCANNER_HEADERS}
    ${KV_SCANNER_HEADERS}
    ${XML_SCANNER_HEADERS}
    ${STR_REPR_HEADERS}
    ${TIMEUTILS_HEADERS}
    ${LOGTHRDEST_HEADERS}
    ${LOGTHRSOURCE_HEADERS}
    ${SIGNAL_SLOT_CONNECTOR_HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.h
)

set (LIB_HEADERS
    afinter.h
    alarms.h
    apphook.h
    atomic.h
    atomic-gssize.h
    block-ref-parser.h
    cache.h
    console.h
    cfg.h
    cfg-lexer.h
    cfg-lexer-subst.h
    cfg-args.h
    cfg-block.h
    cfg-block-generator.h
    cfg-grammar-internal.h
    cfg-helpers.h
    cfg-parser.h
    cfg-path.h
    cfg-source.h
    cfg-tree.h
    cfg-walker.h
    cfg-persist.h
    children.h
    crypto.h
    dnscache.h
    driver.h
    dynamic-window-pool.h
    dynamic-window.h
    fdhelpers.h
    file-monitor.h
    file-perms.h
    find-crlf.h
    generic-number.h
    gprocess.h
    gsockaddr.h
    gsocket.h
    hostname.h
    host-resolve.h
    list-adt.h
    logmatcher.h
    logmpx.h
    logpipe.h
    logqueue-fifo.h
    logqueue.h
    logreader.h
    logsource.h
    logscheduler.h
    logscheduler-pipe.h
    logwriter.h
    mainloop.h
    mainloop-call.h
    mainloop-worker.h
    mainloop-io-worker.h
    mainloop-threaded-worker.h
    module-config.h
    memtrace.h
    messages.h
    metrics-pipe.h
    ml-batched-timer.h
    mainloop-control.h
    msg-format.h
    msg-stats.h
    on-error.h
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
    stackdump.h
    str-format.h
    str-utils.h
    syslog-names.h
    syslog-ng.h
    string-list.h
    tls-support.h
    thread-utils.h
    uuid.h
    userdb.h
    utf8utils.h
    versioning.h
    ringbuffer.h
    host-id.h
    resolved-configurable-paths.h
    window-size-counter.h
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
    console.c
    cfg.c
    cfg-args.c
    cfg-block.c
    cfg-block-generator.c
    cfg-lexer.c
    cfg-lexer-subst.c
    cfg-grammar-internal.c
    cfg-helpers.c
    cfg-parser.c
    cfg-path.c
    cfg-source.c
    cfg-tree.c
    cfg-walker.c
    cfg-persist.c
    children.c
    dnscache.c
    driver.c
    dynamic-window.c
    dynamic-window-pool.c
    fdhelpers.c
    file-monitor.c
    file-perms.c
    find-crlf.c
    globals.c
    generic-number.c
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
    logscheduler.c
    logscheduler-pipe.c
    logsource.c
    logwriter.c
    mainloop.c
    signal-handler.c
    mainloop-call.c
    mainloop-worker.c
    mainloop-io-worker.c
    mainloop-threaded-worker.c
    module-config.c
    memtrace.c
    messages.c
    metrics-pipe.c
    ml-batched-timer.c
    mainloop-control.c
    msg-format.c
    msg-stats.c
    on-error.c
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
    stackdump.c
    str-format.c
    str-utils.c
    syslog-names.c
    string-list.c
    ringbuffer.c
    crypto.c
    uuid.c
    userdb.c
    utf8utils.c
    host-id.c
    resolved-configurable-paths.c
    window-size-counter.c
    ${ACK_TRACKER_SOURCES}
    ${COMPAT_SOURCES}
    ${CONTROL_SOURCES}
    ${HEALTHCHECK_SOURCES}
    ${DEBUGGER_SOURCES}
    ${FILTER_SOURCES}
    ${LOGMSG_SOURCES}
    ${LOGPROTO_SOURCES}
    ${MULTI_LINE_SOURCES}
    ${PARSER_SOURCES}
    ${REWRITE_SOURCES}
    ${STATS_SOURCES}
    ${METRICS_SOURCES}
    ${TEMPLATE_SOURCES}
    ${TRANSPORT_SOURCES}
    ${VALUE_PAIRS_SOURCES}
    ${SCANNER_SOURCES}
    ${STR_REPR_SOURCES}
    ${TIMEUTILS_SOURCES}
    ${LOGTHRDEST_SOURCES}
    ${LOGTHRSOURCE_SOURCES}
    ${SIGNAL_SLOT_CONNECTOR_SOURCES}
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
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/lib/block-ref-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS})

generate_y_from_ym (lib/pragma-grammar)
bison_target(PragmaGrammar
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.y
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer PragmaGrammar)
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/lib/pragma-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS})

generate_y_from_ym (lib/filter/filter-expr-grammar)
bison_target(FilterExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer FilterExprGrammar)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/filter/filter-expr-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS})

generate_y_from_ym (lib/rewrite/rewrite-expr-grammar)
bison_target(RewriteExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer RewriteExprGrammar)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/rewrite/rewrite-expr-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS})

generate_y_from_ym (lib/parser/parser-expr-grammar)
bison_target(ParserExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
add_flex_bison_dependency(CfgLexer ParserExprGrammar)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/parser/parser-expr-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS})

set(CORE_INCLUDE_DIRS
    ${Gettext_INCLUDE_DIR}
    ${IVYKIS_INCLUDE_DIR}
    ${JSONC_INCLUDE_DIR}
    ${LIBPCRE_INCLUDE_DIRS}
    ${Libsystemd_INCLUDE_DIRS}
    ${LIBUNWIND_INCLUDE_DIRS}
)

add_library(syslog-ng SHARED ${LIB_SOURCES})

target_include_directories(syslog-ng
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(syslog-ng
  SYSTEM PUBLIC
    ${CORE_INCLUDE_DIRS}
)

target_link_libraries(
    syslog-ng
  PUBLIC
    m
    eventlog
    GLib::GLib
    GLib::GThread
    GLib::GModule
    ${Gettext_LIBRARIES}
    ${IVYKIS_LIBRARIES}
    ${JSONC_LIBRARY}
    ${LIBPCRE_LIBRARIES}
    ${Libsystemd_LIBRARIES}
    ${LIBUNWIND_LIBRARIES}
    resolv
    libcap
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    secret-storage
)

set_target_properties(syslog-ng
    PROPERTIES VERSION ${SYSLOG_NG_VERSION}
    SOVERSION ${SYSLOG_NG_VERSION})

gperf_generate(severity-aliases.table ${CMAKE_CURRENT_BINARY_DIR}/severity-aliases.h syslog-ng)

if (${IVYKIS_INTERNAL})
   add_dependencies(syslog-ng IVYKIS)
endif()

install(TARGETS syslog-ng LIBRARY DESTINATION lib)

install(FILES ${LIB_HEADERS} DESTINATION include/syslog-ng)

install(FILES ${VALUE_PAIRS_HEADERS} DESTINATION include/syslog-ng/value-pairs)
install(FILES ${COMPAT_HEADERS} DESTINATION include/syslog-ng/compat)
install(FILES ${CONTROL_HEADERS} DESTINATION include/syslog-ng/control)
install(FILES ${HEALTHCHECK_HEADERS} DESTINATION include/syslog-ng/healthcheck)
install(FILES ${DEBUGGER_HEADERS} DESTINATION include/syslog-ng/debugger)
install(FILES ${FILTER_HEADERS} DESTINATION include/syslog-ng/filter)
install(FILES ${LOGMSG_HEADERS} DESTINATION include/syslog-ng/logmsg)
install(FILES ${LOGPROTO_HEADERS} DESTINATION include/syslog-ng/logproto)
install(FILES ${PARSER_HEADERS} DESTINATION include/syslog-ng/parser)
install(FILES ${REWRITE_HEADERS} DESTINATION include/syslog-ng/rewrite)
install(FILES ${STATS_HEADERS} DESTINATION include/syslog-ng/stats)
install(FILES ${METRICS_HEADERS} DESTINATION include/syslog-ng/metrics)
install(FILES ${TEMPLATE_HEADERS} DESTINATION include/syslog-ng/template)
install(FILES ${TRANSPORT_HEADERS} DESTINATION include/syslog-ng/transport)
install(FILES ${CSV_SCANNER_HEADERS} DESTINATION include/syslog-ng/scanner/csv-scanner)
install(FILES ${LOGTHRDEST_HEADERS} DESTINATION include/syslog-ng/logthrdest)
install(FILES ${LOGTHRSOURCE_HEADERS} DESTINATION include/syslog-ng/logthrsource)
install(FILES ${SIGNAL_SLOT_CONNECTOR_HEADERS} DESTINATION include/syslog-ng/signal-slot-connector)

set(TOOLS
    merge-grammar.py
    cfg-grammar.y
)

install(FILES ${TOOLS} DESTINATION share/syslog-ng/tools)

add_test_subdirectory(tests)
