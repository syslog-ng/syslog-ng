module_switch(ENABLE_PYTHON_MODULES "Enable Python based modules" ENABLE_PYTHON)

if(ENABLE_PYTHON_MODULES AND NOT ENABLE_PYTHON)
  message(FATAL_ERROR "Python based modules was explicitly enabled, but the required syslog-ng Python plugin is disabled.")
endif()

if (NOT ENABLE_PYTHON_MODULES)
  return()
endif ()

add_custom_target(BuildPyModules
    COMMAND ${PYTHON_VENV_EXECUTABLE} setup.py build --build-base=${CMAKE_CURRENT_BINARY_DIR}/build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS BuildPyVirtualEnv
    VERBATIM
)

add_custom_target(SetupPyModules
    COMMAND ${PYTHON_VENV_EXECUTABLE} setup.py install --root=/ --install-lib=${CMAKE_CURRENT_BINARY_DIR}/install/lib/syslog-ng/python
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS BuildPyModules
    VERBATIM
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/ DESTINATION ${CMAKE_INSTALL_PREFIX}
    PATTERN "*.py"
)

install(FILES ${PROJECT_SOURCE_DIR}/requirements.txt DESTINATION "${CMAKE_INSTALL_PREFIX}/${SYSLOG_NG_PYTHON_MODULE_DIR}")

if (BUILD_TESTING)
    add_custom_target(pymodules-unit
                      COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_VENV_EXECUTABLE} -m pytest --showlocals --verbosity=3 ${CMAKE_CURRENT_SOURCE_DIR}/syslogng
                      DEPENDS BuildPyVirtualEnv
                      VERBATIM)
    add_dependencies(check pymodules-unit)
endif()

add_custom_target(pymodules-pep8
                  COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_VENV_EXECUTABLE} -m pycodestyle --ignore=E501,E121,E123,E126,E226,E24,E704,W503,W504,R0917 ${CMAKE_CURRENT_SOURCE_DIR}/syslogng
                  DEPENDS BuildPyVirtualEnv
                  VERBATIM)
add_custom_target(pymodules-pylint
                  COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_VENV_EXECUTABLE} -m pylint -r n --rcfile=${CMAKE_CURRENT_SOURCE_DIR}/pylintrc ${CMAKE_CURRENT_SOURCE_DIR}/syslogng
                  DEPENDS BuildPyVirtualEnv
                  VERBATIM)

add_dependencies(mod-python SetupPyModules)
