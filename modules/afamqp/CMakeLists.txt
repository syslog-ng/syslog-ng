external_or_find_package(RabbitMQ)

if (RabbitMQ_FOUND)
  option(ENABLE_AFAMQ "Enable afamqp module" ON)
else()
  option(ENABLE_AFAMQ "Enable afamqp module" OFF)
endif()

if (ENABLE_AFAMQ)

set(AFAMQ_HEADERS
    "afamqp-parser.h"
    "afamqp.h"
    "${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.h"
)

set(AFAMQ_SOURCES
    "afamqp-parser.c"
    "afamqp.c"
    "${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.c"
)

generate_y_from_ym(modules/afamqp/afamqp-grammar)

bison_target(AMQPGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/afamqp-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
include_directories (${RabbitMQ_INCLUDE_DIR})
add_library(afamqp SHARED ${AFAMQ_SOURCES})
target_link_libraries(afamqp PUBLIC ${RabbitMQ_LIBRARY})
target_link_libraries(afamqp PRIVATE syslog-ng)

if (RabbitMQ_INTERNAL)
   add_dependencies(afamqp RabbitMQ)
endif()

install(TARGETS afamqp LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afamqp)

endif()
