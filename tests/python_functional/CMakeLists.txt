add_custom_target(pytest-self-check
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/python_functional/src)

add_custom_target(pytest-check
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/python_functional/functional_tests --installdir=${CMAKE_INSTALL_PREFIX})
