module_switch(ENABLE_PYTHON_MODULES "Enable Python based modules" ENABLE_PYTHON)

if(ENABLE_PYTHON_MODULES AND NOT ENABLE_PYTHON)
  message(FATAL_ERROR "Python based modules was explicitly enabled, but the required syslog-ng Python plugin is disabled.")
endif()

if(NOT ENABLE_PYTHON)
  return()
endif()

# Unlike other modules, the ENABLE_PYTHON_MODULE option does not disable the entire
# build of this module, it only limits if built-in modules are included or not.
if(ENABLE_PYTHON_MODULES)
  set(PYMODULES_BUILTINS_ONLY 0)
else()
  set(PYMODULES_BUILTINS_ONLY 1)
endif()

# setup.py and install() need real paths, not the ${prefix} placeholders that
# lib/reloc.c expands at runtime.
string(REPLACE "\${prefix}" "${CMAKE_INSTALL_PREFIX}" PYMODULES_EXEC_PREFIX "${SYSLOG_NG_PATH_EXECPREFIX}")
string(REPLACE "\${exec_prefix}" "${PYMODULES_EXEC_PREFIX}" PYMODULES_INSTALL_DIR "${SYSLOG_NG_PYTHON_MODULE_DIR}")
string(REPLACE "\${prefix}" "${CMAKE_INSTALL_PREFIX}" PYMODULES_SYSCONF_INSTALL_DIR "${SYSLOG_NG_PYTHON_SYSCONF_MODULE_DIR}")

set(PYMODULES_PATH "modules/python-modules")
set(PYMODULES_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}")
set(PYMODULES_SRCDIR "${CMAKE_SOURCE_DIR}/${PYMODULES_PATH}")
set(PYMODULES_MANIFEST "${PYMODULES_BUILDDIR}/install-manifest.txt")
set(PYMODULES_ROOT_MODULE "syslogng")
set(PYMODULES_SETUP_OPTIONS "--root=\"\${DESTDIR:-/}\" --install-lib=\"${PYMODULES_INSTALL_DIR}\"")

# Equivalent of 'pymodules-install-requirements'
add_custom_target(InstallPyModulesRequirements
  COMMAND /bin/sh -c "\
          if [ '${PYTHON_PACKAGES_INSTALL}' != 'none' ]; then \
              echo 'Installing Python module requirements...'; \
              mkdir -p ${PYMODULES_BUILDDIR}; \
              ${PYTHON_VENV_EXECUTABLE} setup.py egg_info --egg-base=${PYMODULES_BUILDDIR}; \
              if [ -f '${PYMODULES_BUILDDIR}/${PYMODULES_ROOT_MODULE}.egg-info/requires.txt' ]; then \
                '${PYTHON_VENV_EXECUTABLE}' -m pip install -r '${PYMODULES_BUILDDIR}/${PYMODULES_ROOT_MODULE}.egg-info/requires.txt'; \
              fi; \
          else \
              echo 'NOT Installing Python module requirements'; \
          fi"
  WORKING_DIRECTORY ${PYMODULES_SRCDIR}
  DEPENDS BuildPyVirtualEnv
  VERBATIM
)

# Equivalent of 'pymodules-install'
add_custom_target(InstallPyModules
  COMMAND /bin/sh -c "echo \"Building and installing Python modules (with\$( [ ${PYMODULES_BUILTINS_ONLY} -eq 1 ] && echo 'out') built-ins)...\""
  COMMAND /bin/sh -c "PYMODULES_BUILTINS_ONLY=${PYMODULES_BUILTINS_ONLY} \
                      '${PYTHON_VENV_EXECUTABLE}' setup.py \
                      build --build-base='${PYMODULES_BUILDDIR}/build' \
                      egg_info --egg-base='${PYMODULES_BUILDDIR}' \
                      install --record='${PYMODULES_MANIFEST}' \
                      ${PYMODULES_SETUP_OPTIONS}"

  WORKING_DIRECTORY ${PYMODULES_SRCDIR}
  DEPENDS InstallPyModulesRequirements
  VERBATIM
)

# Run Python module install only during installation phase
install(CODE "
  execute_process(
    COMMAND /bin/sh -c \"${CMAKE_COMMAND} --build . --target InstallPyModules\"
    WORKING_DIRECTORY \"${CMAKE_BINARY_DIR}\"
  )
")

# Install artifacts
install(FILES ${PYMODULES_SRCDIR}/README.md
  DESTINATION "${PYMODULES_SYSCONF_INSTALL_DIR}")

install(FILES ${PROJECT_SOURCE_DIR}/requirements.txt
  DESTINATION "${PYMODULES_INSTALL_DIR}")

if(BUILD_TESTING)
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
