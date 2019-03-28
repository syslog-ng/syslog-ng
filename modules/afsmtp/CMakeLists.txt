set(AFSMTP_SOURCES
    afsmtp.c
    afsmtp.h
    afsmtp-parser.c
    afsmtp-parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/afsmtp-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/afsmtp-grammar.c
)

find_package(ESMTP)

bison_target(AfsmtpGrammar
  ${CMAKE_CURRENT_BINARY_DIR}/afsmtp-grammar.y
  ${CMAKE_CURRENT_BINARY_DIR}/afsmtp-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})

generate_y_from_ym(modules/afsmtp/afsmtp-grammar)

module_switch(ENABLE_AFSMTP "Enable SMTP destination" ESMTP_FOUND)
if (ENABLE_AFSMTP)
    add_library(afsmtp MODULE ${AFSMTP_SOURCES})
    target_link_libraries (afsmtp PUBLIC ${ESMTP_LIBRARIES})
    target_include_directories (afsmtp SYSTEM PRIVATE ${ESMTP_INCLUDE_DIR})
    target_include_directories (afsmtp PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(afsmtp PRIVATE syslog-ng)

    install(TARGETS afsmtp LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afsmtp)
endif()
