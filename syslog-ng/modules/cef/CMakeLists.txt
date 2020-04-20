set(CEF_SOURCES
    format-cef-extension.c
    format-cef-extension.h
    cef-plugin.c
)

add_module(
  TARGET cef
  SOURCES ${CEF_SOURCES}
)

add_test_subdirectory(tests)
