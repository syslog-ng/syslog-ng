
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

if (BUILD_TESTING)
    add_custom_target(pymodules-unit
                      COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_VENV_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/syslogng
                      DEPENDS BuildPyVirtualEnv
                      VERBATIM)
    add_dependencies(check pymodules-unit)
endif()

add_custom_target(pymodules-pep8
                  COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_VENV_EXECUTABLE} -m pep8 --ignore=E501 ${CMAKE_CURRENT_SOURCE_DIR}/syslogng
                  DEPENDS BuildPyVirtualEnv
                  VERBATIM)
add_custom_target(pymodules-pylint
                  COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_VENV_EXECUTABLE} -m pylint -r n --rcfile=${CMAKE_CURRENT_SOURCE_DIR}/pylintrc ${CMAKE_CURRENT_SOURCE_DIR}/syslogng
                  DEPENDS BuildPyVirtualEnv
                  VERBATIM)

add_dependencies(mod-python SetupPyModules)
