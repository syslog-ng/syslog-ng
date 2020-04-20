module_switch(ENABLE_APPMODEL "Enable AppModel")

if (NOT ENABLE_APPMODEL)
  return()
endif()

set (APPMODEL_SOURCES
    appmodel.c
    appmodel-parser.c
    appmodel-plugin.c
    appmodel-context.c
    app-parser-generator.c
    application.c
)

add_module(
  TARGET appmodel
  GRAMMAR appmodel-grammar
  SOURCES ${APPMODEL_SOURCES}
)

add_test_subdirectory(tests)

