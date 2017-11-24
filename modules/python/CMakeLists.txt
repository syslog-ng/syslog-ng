
find_package(PythonLibs)

set(PYTHON_SOURCES
    python-module.h
    python-config.h
    python-config.c
    python-helpers.h
    python-helpers.c
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
    python-debugger.c
    python-debugger.h
    python-logparser.h
    python-logparser.c
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.h
)

generate_y_from_ym(modules/python/python-grammar)

bison_target(PythonGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

if (PYTHONLIBS_FOUND)
    option(ENABLE_PYTHON "Enable Python module" ON)
else()
    option(ENABLE_PYTHON "Enable Python module" OFF)
endif()

if (ENABLE_PYTHON)
    add_subdirectory(pylib)
    add_library(mod-python MODULE ${PYTHON_SOURCES})
    target_include_directories (mod-python PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories (mod-python PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (mod-python PRIVATE ${PYTHON_INCLUDE_DIRS} ${CORE_INCLUDE_DIRS})
    target_link_libraries(mod-python PRIVATE syslog-ng ${PYTHON_LIBRARIES} ${Eventlog_LIBRARIES} ${IVYKIS_LIBRARIES})
    add_dependencies(mod-python SetupPylib)
    install(TARGETS mod-python DESTINATION lib/syslog-ng/)
endif()
