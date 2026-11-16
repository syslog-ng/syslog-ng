set(syslog_ng_bin_sources
    main.c)

add_executable(syslog-ng-bin ${syslog_ng_bin_sources})
SET_TARGET_PROPERTIES(syslog-ng-bin PROPERTIES OUTPUT_NAME syslog-ng)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib/)
target_link_libraries(syslog-ng-bin syslog-ng)

if(ENABLE_ENV_WRAPPER)
  install(TARGETS syslog-ng-bin RUNTIME DESTINATION libexec)

  # Intentionally not linked against syslog-ng: must start even when the
  # real binary's shared libs cannot be found without LD_LIBRARY_PATH set.
  # Keeps its own build-tree output name to avoid colliding with syslog-ng-bin's
  # OUTPUT_NAME; renamed to syslog-ng only at install time.
  add_executable(syslog-ng-wrapper wrapper.c)
  target_include_directories(syslog-ng-wrapper PRIVATE ${PROJECT_SOURCE_DIR}/lib ${GLIB_INCLUDE_DIRS})
  install(PROGRAMS $<TARGET_FILE:syslog-ng-wrapper> DESTINATION sbin RENAME syslog-ng)
else()
  install(TARGETS syslog-ng-bin RUNTIME DESTINATION sbin)
endif()
