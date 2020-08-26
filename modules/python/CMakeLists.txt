module_switch(ENABLE_PYTHON "Enable Python module" PYTHONLIBS_FOUND)
if (NOT ENABLE_PYTHON)
  return()
endif ()

if (NOT PYTHONLIBS_FOUND)
  message(FATAL_ERROR "Python module enabled, but python library not found.")
endif()

set(PYTHON_SOURCES
    python-module.h
    python-config.h
    python-config.c
    python-persist.h
    python-persist.c
    python-helpers.h
    python-helpers.c
    python-http-header.h
    python-http-header.c
    python-main.h
    python-main.c
    python-plugin.c
    python-value-pairs.c
    python-value-pairs.h
    python-dest.c
    python-dest.h
    python-tf.c
    python-tf.h
    python-parser.c
    python-parser.h
    python-logmsg.h
    python-logmsg.c
    python-logtemplate.h
    python-logtemplate.c
    python-logtemplate-options.h
    python-logtemplate-options.c
    python-global-code-loader.h
    python-global-code-loader.c
    python-debugger.c
    python-debugger.h
    python-logparser.h
    python-logparser.c
    python-integerpointer.h
    python-integerpointer.c
    python-logger.h
    python-logger.c
    python-source.h
    python-source.c
    python-fetcher.h
    python-fetcher.c
    python-bookmark.h
    python-bookmark.c
    python-ack-tracker.h
    python-ack-tracker.c
    compat/compat-python.c
)

add_module(
  TARGET mod-python
  GRAMMAR python-grammar
  INCLUDES ${PYTHON_INCLUDE_DIRS}
           ${PROJECT_SOURCE_DIR}
  DEPENDS ${PYTHON_LIBRARIES}
          ${Eventlog_LIBRARIES}
          ${IVYKIS_LIBRARIES}
  SOURCES ${PYTHON_SOURCES}
)

add_subdirectory(pylib)
add_dependencies(mod-python SetupPylib)

add_test_subdirectory(tests)
