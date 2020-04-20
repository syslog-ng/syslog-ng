set(AFPROG_SOURCES
    "afprog.h"
    "afprog-parser.h"
    "afprog.c"
    "afprog-parser.c"
    "afprog-plugin.c"
)

add_module(
  TARGET afprog
  GRAMMAR afprog-grammar
  SOURCES ${AFPROG_SOURCES}
)

