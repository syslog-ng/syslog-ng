add_custom_target(BuildPylib
    COMMAND python setup.py build --build-base=${CMAKE_CURRENT_BINARY_DIR}/build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

add_custom_target(SetupPylib
    COMMAND python setup.py install --root=/ --prefix=${CMAKE_CURRENT_BINARY_DIR}/install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS BuildPylib
    VERBATIM
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/ DESTINATION ${CMAKE_INSTALL_PREFIX}
    PATTERN "*.py"
)
