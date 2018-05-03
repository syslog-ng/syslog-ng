set(HOOK_COMMANDS_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/hook-commands-grammar.h"
    "hook-commands-parser.h"
    "hook-commands.h"
)

set(HOOK_COMMANDS_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/hook-commands-grammar.c"
    "hook-commands-plugin.c"
    "hook-commands-parser.c"
    "hook-commands.c"
)

generate_y_from_ym(modules/hook-commands/hook-commands-grammar)

option(ENABLE_HOOK_COMMANDS "Enable hook-commands module" ON)

if (ENABLE_HOOK_COMMANDS)
  bison_target(HookCommandsGrammar
      ${CMAKE_CURRENT_BINARY_DIR}/hook-commands-grammar.y
      ${CMAKE_CURRENT_BINARY_DIR}/hook-commands-grammar.c
      COMPILE_FLAGS ${BISON_FLAGS})

  add_library(hook-commands MODULE ${HOOK_COMMANDS_SOURCES})
  target_include_directories(hook-commands
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(hook-commands PRIVATE syslog-ng)

  install(TARGETS hook-commands LIBRARY DESTINATION lib/syslog-ng/ COMPONENT hook-commands)
endif()

