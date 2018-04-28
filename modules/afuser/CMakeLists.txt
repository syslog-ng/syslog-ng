set(AFUSER_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/afuser-grammar.h"
    "afuser-parser.h"
    "afuser.h"
    )

set(AFUSER_SOURCES
	"afuser-plugin.c"
	"afuser.c"
    "${CMAKE_CURRENT_BINARY_DIR}/afuser-grammar.c"
    "afuser-parser.c"
    )

generate_y_from_ym(modules/afuser/afuser-grammar)

bison_target(AfuserGrammar
  ${CMAKE_CURRENT_BINARY_DIR}/afuser-grammar.y
  ${CMAKE_CURRENT_BINARY_DIR}/afuser-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})

option(ENABLE_AFUSER "Enable afuser module" ON)

if (ENABLE_AFUSER)
  add_library(afuser MODULE ${AFUSER_SOURCES})
  target_include_directories(afuser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(afuser PRIVATE syslog-ng)

  install(TARGETS afuser LIBRARY DESTINATION lib/syslog-ng COMPONENT afuser)
endif()
