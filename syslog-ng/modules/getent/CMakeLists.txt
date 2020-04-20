module_switch(ENABLE_GETENT "Enable getent")

if (NOT ENABLE_GETENT)
  return ()
endif()

set(GETENT_SOURCES
  tfgetent.c)

add_module(
  TARGET tfgetent
  SOURCES ${GETENT_SOURCES}
)

