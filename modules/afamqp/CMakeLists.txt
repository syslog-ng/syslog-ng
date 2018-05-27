find_package(RabbitMQ)

if (RabbitMQ_FOUND)
  option(ENABLE_AFAMQP "Enable afamqp module" ON)
else()
  option(ENABLE_AFAMQP "Enable afamqp module" OFF)
endif()

if (ENABLE_AFAMQP)

set(AFAMQP_HEADERS
    "afamqp-parser.h"
    "afamqp.h"
    "${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.h"
)

set(AFAMQP_SOURCES
    "afamqp-parser.c"
    "afamqp.c"
    "${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.c"
)

generate_y_from_ym(modules/afamqp/afamqp-grammar)

bison_target(AMQPGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${RabbitMQ_INCLUDE_DIR})
add_library(afamqp SHARED ${AFAMQP_SOURCES})
target_include_directories(afamqp
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(afamqp PRIVATE ${RabbitMQ_LIBRARY} syslog-ng)

install(TARGETS afamqp LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afamqp)

endif()
