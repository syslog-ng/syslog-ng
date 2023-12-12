module_switch(ENABLE_EXAMPLE_FILTERX_FUNC "Enable example-filterx-func")
if (NOT ENABLE_EXAMPLE_FILTERX_FUNC)
  return()
endif()

set(EXAMPLE_FILTERX_FUNC_SOURCES
    example-filterx-func-plugin.c
)

add_module(
  TARGET example_filterx_func
  SOURCES ${EXAMPLE_FILTERX_FUNC_SOURCES}
  LIBRARY_TYPE STATIC
)
