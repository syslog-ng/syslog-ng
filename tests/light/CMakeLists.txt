add_custom_target(light-self-check
   COMMAND ${PYTHON_VENV_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/light/src --showlocals --verbosity=3
   DEPENDS BuildPyVirtualEnv)

add_custom_target(light-check
   COMMAND ${PYTHON_VENV_EXECUTABLE} -m pytest ${PROJECT_SOURCE_DIR}/tests/light/functional_tests --installdir=${CMAKE_INSTALL_PREFIX} --showlocals --verbosity=3
   DEPENDS BuildPyVirtualEnv)

add_custom_target(light-linters
   COMMAND find ${PROJECT_SOURCE_DIR}/tests/light/ -name "*.py" -not -path "*reports*" -not -path "*tox*" -print0 | xargs -0 ${PYTHON_VENV_EXECUTABLE} -m pre_commit run --show-diff-on-failure --config=${PROJECT_SOURCE_DIR}/tests/light/.pre-commit-config.yaml --files WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
   DEPENDS BuildPyVirtualEnv)
