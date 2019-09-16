add_custom_target(pytest-self-check
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/python_functional/src --showlocals --verbosity=3)

add_custom_target(pytest-check
   COMMAND ${PYTHON_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/python_functional/functional_tests --installdir=${CMAKE_INSTALL_PREFIX} --showlocals --verbosity=3)

add_custom_target(pytest-linters
   COMMAND find ${PROJECT_SOURCE_DIR}/tests/python_functional/ -name "*.py" -not -path "*reports*" -not -path "*tox*" -print0 | xargs -0 pre-commit run --show-diff-on-failure --config=${PROJECT_SOURCE_DIR}/tests/python_functional/.pre-commit-config.yaml --files WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
