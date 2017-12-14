add_custom_target(func-test
  ${CMAKE_COMMAND} -E env "top_srcdir=${CMAKE_SOURCE_DIR}" "top_builddir=${CMAKE_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/runtests.sh")
