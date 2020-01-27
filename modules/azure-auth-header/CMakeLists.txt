set(AZURE_AUTH_HEADER_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/azure-auth-header-grammar.h"
    "azure-auth-header-parser.h"
    "azure-auth-header-plugin.h"
    "azure-auth-header.h"
)

set(AZURE_AUTH_HEADER_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/azure-auth-header-grammar.c"
    "azure-auth-header-plugin.c"
    "azure-auth-header-parser.c"
    "azure-auth-header.c"
)

generate_y_from_ym(modules/azure-auth-header/azure-auth-header-grammar)

module_switch(ENABLE_AZURE_AUTH_HEADER "Enable azure-auth-header module")

if (ENABLE_AZURE_AUTH_HEADER)
  bison_target(AzureAuthHeaderGrammar
      ${CMAKE_CURRENT_BINARY_DIR}/azure-auth-header-grammar.y
      ${CMAKE_CURRENT_BINARY_DIR}/azure-auth-header-grammar.c
      COMPILE_FLAGS ${BISON_FLAGS})

  add_library(azure-auth-header MODULE ${AZURE_AUTH_HEADER_SOURCES})
  target_include_directories(azure-auth-header
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${PROJECT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )

  target_link_libraries(azure-auth-header PRIVATE syslog-ng)

  install(TARGETS azure-auth-header LIBRARY DESTINATION lib/syslog-ng/ COMPONENT azure-auth-header)
endif()
