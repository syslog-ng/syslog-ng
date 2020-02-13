find_package(RiemannClient)
if (HAVE_RIEMANN_MICROSECONDS)
  add_definitions("-DSYSLOG_NG_HAVE_RIEMANN_MICROSECONDS")
endif()

module_switch(ENABLE_RIEMANN "Enable riemann destination" Riemann_FOUND)
if (NOT ENABLE_RIEMANN)
  return()
endif()

if (NOT Riemann_FOUND)
  message(FATAL_ERROR "Riemann enabled but riemann-c-client is missing!")
endif()

set (RIEMANN_SOURCES
    riemann-grammar.y
    riemann.c
    riemann.h
    riemann-worker.c
    riemann-worker.h
    riemann-parser.c
    riemann-parser.h
    riemann-plugin.c
)

add_module(
  TARGET riemann
  GRAMMAR riemann-grammar
  INCLUDES ${Riemann_INCLUDE_DIR}
  DEPENDS ${Riemann_LIBRARIES}
  SOURCES ${RIEMANN_SOURCES}
)

