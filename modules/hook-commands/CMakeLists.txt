module_switch(ENABLE_HOOK_COMMANDS "Enable hook-commands module")

if (NOT ENABLE_HOOK_COMMANDS)
  return()
endif()

set(HOOK_COMMANDS_SOURCES
    "hook-commands-parser.h"
    "hook-commands.h"
    "hook-commands-plugin.c"
    "hook-commands-parser.c"
    "hook-commands.c"
)

add_module(
  TARGET hook-commands
  GRAMMAR hook-commands-grammar
  SOURCES ${HOOK_COMMANDS_SOURCES}
)

