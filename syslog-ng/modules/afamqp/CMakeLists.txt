module_switch(ENABLE_AFAMQP "Enable afamqp module" RabbitMQ_FOUND)
if (ENABLE_AFAMQP)

set(AFAMQP_SOURCES
    "afamqp-parser.h"
    "afamqp.h"
    "compat/amqp-compat.h"
    "afamqp-parser.c"
    "afamqp-plugin.c"
    "afamqp.c"
    "compat/amqp-compat.c"
)

add_module(
  TARGET afamqp
  GRAMMAR afamqp-grammar
  INCLUDES ${RabbitMQ_INCLUDE_DIR}
  DEPENDS ${RabbitMQ_LIBRARY}
  SOURCES ${AFAMQP_SOURCES}
)

endif()
