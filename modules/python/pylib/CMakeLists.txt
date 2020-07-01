add_custom_target(BuildPylib
    COMMAND ${PYTHON_EXECUTABLE} setup.py build --build-base=${CMAKE_CURRENT_BINARY_DIR}/build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

add_custom_target(SetupPylib
    COMMAND ${PYTHON_EXECUTABLE} setup.py install --root=/ --install-lib=${CMAKE_CURRENT_BINARY_DIR}/install/lib/syslog-ng/python
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS BuildPylib
    VERBATIM
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/ DESTINATION ${CMAKE_INSTALL_PREFIX}
    PATTERN "*.py"
)

if (BUILD_TESTING)
    execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE IS_PYTEST_INSTALLED)
    if(IS_PYTEST_INSTALLED EQUAL "0")
        add_custom_target(test_pylib_unit COMMAND ${CMAKE_COMMAND} -E env ${PYTHON_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/syslogng)
        add_dependencies(check test_pylib_unit)
    endif()
endif()

add_custom_target(python-pep8 COMMAND ${CMAKE_COMMAND} -E env pep8 --ignore=E501 ${CMAKE_CURRENT_SOURCE_DIR}/syslogng)
add_custom_target(python-pylint COMMAND ${CMAKE_COMMAND} -E env ${PYLINT_BIN} -r n --rcfile=${CMAKE_CURRENT_SOURCE_DIR}/pylintrc ${CMAKE_CURRENT_SOURCE_DIR}/syslogng)
