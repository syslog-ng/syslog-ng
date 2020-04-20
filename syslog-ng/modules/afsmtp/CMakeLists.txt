find_package(ESMTP)

module_switch(ENABLE_AFSMTP "Enable SMTP destination" ESMTP_FOUND)

if (NOT ENABLE_AFSMTP)
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

