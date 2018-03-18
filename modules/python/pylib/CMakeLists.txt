add_custom_target(BuildPylib
    COMMAND ${PYTHON_EXECUTABLE} setup.py build --build-base=${CMAKE_CURRENT_BINARY_DIR}/build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

add_custom_target(SetupPylib
    COMMAND ${PYTHON_EXECUTABLE} setup.py install --root=/ --prefix=${CMAKE_CURRENT_BINARY_DIR}/install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS BuildPylib
    VERBATIM
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/ DESTINATION ${CMAKE_INSTALL_PREFIX}
    PATTERN "*.py"
)

if (BUILD_TESTING)
add_test(test_pylib_unit ${CMAKE_COMMAND} -E env nosetests ${CMAKE_CURRENT_SOURCE_DIR}/syslogng)
add_test(test_pylib_pycodestyle ${CMAKE_COMMAND} -E env pycodestyle --ignore=E501 ${CMAKE_CURRENT_SOURCE_DIR}/syslogng)
add_test(test_pylib_pylint ${CMAKE_COMMAND} -E env pylint -r n --rcfile=${CMAKE_CURRENT_SOURCE_DIR}/pylintrc ${CMAKE_CURRENT_SOURCE_DIR}/syslogng)
endif()
