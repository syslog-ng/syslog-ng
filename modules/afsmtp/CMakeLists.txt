if(NOT DEFINED ENABLE_AFSMTP OR ENABLE_AFSMTP)
  find_package(ESMTP)
endif()

module_switch(ENABLE_AFSMTP "Enable SMTP destination" ESMTP_FOUND)

if(ENABLE_AFSMTP AND NOT ESMTP_FOUND)
  message(FATAL_ERROR "SMTP module was explicitly enabled, but the required libesmtp dependency could not be found")
endif()

if(NOT ENABLE_AFSMTP)
  return()
endif()

set(AFSMTP_SOURCES
  afsmtp.c
  afsmtp.h
  afsmtp-parser.c
  afsmtp-parser.h
)

add_module(
  TARGET afsmtp
  GRAMMAR afsmtp-grammar
  INCLUDES ${ESMTP_INCLUDE_DIR}
  DEPENDS ${ESMTP_LIBRARIES}
  SOURCES ${AFSMTP_SOURCES}
)
