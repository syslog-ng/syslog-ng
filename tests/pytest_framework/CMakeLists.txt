add_custom_target(pytest-self-check
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/pytest_framework/src)

add_custom_target(pytest-check
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/pytest_framework/functional_tests --installdir=${CMAKE_INSTALL_PREFIX})

add_custom_target(pytest-check-all
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/pytest_framework/functional_tests --runslow --installdir=${CMAKE_INSTALL_PREFIX})
