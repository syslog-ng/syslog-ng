module_switch(ENABLE_AZURE_AUTH_HEADER "Enable azure-auth-header module")

if (NOT ENABLE_AZURE_AUTH_HEADER)
  return()
endif()


set(AZURE_AUTH_HEADER_SOURCES
    "azure-auth-header-parser.h"
    "azure-auth-header.h"
    "azure-auth-header-plugin.c"
    "azure-auth-header-parser.c"
    "azure-auth-header.c"
)

add_module(
  TARGET azure-auth-header
  GRAMMAR azure-auth-header-grammar
  INCLUDES ${PROJECT_SOURCE_DIR}
  SOURCES ${AZURE_AUTH_HEADER_SOURCES}
)

