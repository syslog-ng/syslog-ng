if(NOT DEFINED ENABLE_AFAMQP OR ENABLE_AFAMQP)
  find_package(RabbitMQ)
endif()

module_switch(ENABLE_AFAMQP "Enable afamqp module" RabbitMQ_FOUND)

if(ENABLE_AFAMQP AND NOT RabbitMQ_FOUND)
  message(FATAL_ERROR "MQTTT module was explicitly enabled, but the required RabbitMQ dependency could not be found")
endif()

if(NOT ENABLE_AFAMQP)
  return()
endif()

set(CMAKE_REQUIRED_INCLUDES ${RabbitMQ_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${RabbitMQ_LIBRARY})
check_include_file("rabbitmq-c/tcp_socket.h" SYSLOG_NG_HAVE_RABBITMQ_C_TCP_SOCKET_H)
check_symbol_exists(amqp_ssl_socket_set_verify_peer "amqp.h;amqp_ssl_socket.h" SYSLOG_NG_HAVE_AMQP_SSL_SOCKET_SET_VERIFY_PEER)

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
