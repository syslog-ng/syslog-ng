set(AFSQL_SOURCES
    afsql.c
    afsql.h
    afsql-parser.c
    afsql-parser.h
    afsql-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/afsql-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/afsql-grammar.c
)

find_package(LibDBI)
find_package(OpenSSL)

module_switch(ENABLE_SQL "Enable SQL plugin" LIBDBI_FOUND)
if (ENABLE_SQL)
    generate_y_from_ym(modules/afsql/afsql-grammar)

    bison_target(SqlGrammar
        ${CMAKE_CURRENT_BINARY_DIR}/afsql-grammar.y
        ${CMAKE_CURRENT_BINARY_DIR}/afsql-grammar.c
        COMPILE_FLAGS ${BISON_FLAGS})

    add_library(afsql MODULE ${AFSQL_SOURCES})
    target_link_libraries (afsql PUBLIC ${LIBDBI_LIBRARIES} OpenSSL::SSL)
    target_include_directories (afsql SYSTEM PRIVATE ${LIBDBI_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
    target_include_directories (afsql PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(afsql PRIVATE syslog-ng)

    install(TARGETS afsql LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afsql)
endif()
