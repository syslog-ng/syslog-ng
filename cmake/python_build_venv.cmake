
set(PYTHON_VENV_DIR "${PROJECT_BINARY_DIR}/venv")
set(PYTHON_VENV_TOUCHFILE "${PROJECT_BINARY_DIR}/.python-venv-built")
set(PYTHON_VENV_EXECUTABLE "${PYTHON_VENV_DIR}/bin/python")

add_custom_command(
    OUTPUT ${PYTHON_VENV_TOUCHFILE}
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/build-python-venv.sh "${PYTHON_EXECUTABLE}" "${PROJECT_BINARY_DIR}/venv" "${PROJECT_SOURCE_DIR}" && touch "${PYTHON_VENV_TOUCHFILE}"
    VERBATIM
)

add_custom_target(BuildPyVirtualEnv
                  DEPENDS ${PYTHON_VENV_TOUCHFILE})
