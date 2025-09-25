if(NOT DEFINED ENABLE_RIEMANN OR ENABLE_RIEMANN)
  find_package(RiemannClient)
endif()

module_switch(ENABLE_RIEMANN "Enable riemann destination" Riemann_FOUND)

if(ENABLE_RIEMANN AND NOT Riemann_FOUND)
  message(FATAL_ERROR "Riemann module was explicitly enabled, but the required riemann-c-client dependency could not be found")
endif()

if(NOT ENABLE_RIEMANN)
  return()
endif()

if(HAVE_RIEMANN_MICROSECONDS)
  set(SYSLOG_NG_HAVE_RIEMANN_MICROSECONDS 1 CACHE BOOL "Riemann Microseconds support" FORCE)
endif()

set(RIEMANN_SOURCES
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
