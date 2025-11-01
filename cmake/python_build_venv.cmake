# --- Python package handling mode (venv / system / none) ---
set(PYTHON_PACKAGES_INSTALL "venv" CACHE STRING "Python package installation mode (venv, system, none)")

# Configure venv/system/none behaviour
if(PYTHON_PACKAGES_INSTALL STREQUAL "none")
    set(PYTHON_VENV_EXECUTABLE "${PYTHON_EXECUTABLE}")
elseif(PYTHON_PACKAGES_INSTALL STREQUAL "system")
    set(PYTHON_VENV_EXECUTABLE "${PYTHON_EXECUTABLE}")
elseif(PYTHON_PACKAGES_INSTALL STREQUAL "venv")
    set(PYTHON_VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/venv")
    set(PYTHON_VENV_EXECUTABLE "${PYTHON_VENV_DIR}/bin/python")
else()
    message(FATAL_ERROR "Unknown PYTHON_PACKAGES_INSTALL='${PYTHON_PACKAGES_INSTALL}', valid values are: venv, system, none")
endif()

set(PYTHON_VENV_TOUCHFILE "${PROJECT_BINARY_DIR}/.python-venv-built")

add_custom_command(
    OUTPUT ${PYTHON_VENV_TOUCHFILE}
    COMMAND /bin/sh -c "\
            if [ \"${PYTHON_PACKAGES_INSTALL}\" = \"venv\" ]; then \
                '${PROJECT_SOURCE_DIR}/scripts/build-python-venv.sh' '${PYTHON_EXECUTABLE}' '${PROJECT_BINARY_DIR}/venv' '${PROJECT_SOURCE_DIR}'; \
            fi; \
            touch '${PYTHON_VENV_TOUCHFILE}'"
    VERBATIM
)

if(PYTHON_PACKAGES_INSTALL STREQUAL "venv")
    # the touchfile is cleaned up by cmake automatically, we only need to remove
    # the venv directory
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${PYTHON_VENV_DIR}")
endif()

add_custom_target(BuildPyVirtualEnv
    DEPENDS ${PYTHON_VENV_TOUCHFILE})
