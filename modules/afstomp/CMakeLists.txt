module_switch(ENABLE_STOMP "Enable STOMP destination")

if (NOT ENABLE_STOMP)
  return()
endif()

set(AFSTOMP_SOURCES
    afstomp.c
    afstomp.h
    afstomp-parser.c
    afstomp-parser.h
    stomp.c
    stomp.h
)


add_module(
  TARGET afstomp
  GRAMMAR afstomp-grammar
  SOURCES ${AFSTOMP_SOURCES}
)

add_test_subdirectory(tests)
