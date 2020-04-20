module_switch(ENABLE_AFUSER "Enable afuser module")

if (NOT ENABLE_AFUSER)
  return()
endif()

set(AFUSER_SOURCES
  "afuser-parser.h"
  "afuser.h"
	"afuser-plugin.c"
	"afuser.c"
  "afuser-parser.c"
  )

add_module(
  TARGET afuser
  GRAMMAR afuser-grammar
  SOURCES ${AFUSER_SOURCES}
)

