set(AFSTOMP_SOURCES
    afstomp.c
    afstomp.h
    afstomp-parser.c
    afstomp-parser.h
    stomp.c
    stomp.h
    ${CMAKE_CURRENT_BINARY_DIR}/afstomp-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/afstomp-grammar.c
)

set(AFSTOMP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(ENABLE_STOMP "Enable STOMP destination" ON)

if (ENABLE_STOMP)
    generate_y_from_ym(modules/afstomp/afstomp-grammar)

    bison_target(StompGrammar
        ${CMAKE_CURRENT_BINARY_DIR}/afstomp-grammar.y
        ${CMAKE_CURRENT_BINARY_DIR}/afstomp-grammar.c
        COMPILE_FLAGS ${BISON_FLAGS})

    add_library(afstomp SHARED ${AFSTOMP_SOURCES})
    target_include_directories (afstomp PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(afstomp PRIVATE syslog-ng)

    install(TARGETS afstomp LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afstomp)

    add_test_subdirectory(tests)
endif()
