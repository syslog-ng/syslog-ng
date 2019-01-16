set(PERSIST-TOOL_SOURCE
 	add.c
	dump.c
	generate.c
	persist-tool.c
	state.c
  )

add_executable(persist-tool ${PERSIST-TOOL_SOURCE})

include_directories(persist-tool
  ${CORE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GLIB_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/lib
  ${JSONC_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/lib/eventlog/src
  )

target_link_libraries(persist-tool
  ${GLIB_GMODULE_LIBRARIES}
  ${GLIB_GTHREAD_LIBRARIES}
  ${GLIB_LIBRARIES}  
  ${JSONC_LIBRARY}
  syslog-ng
  )

if (JSONC_INTERNAL)
  add_dependencies(persist-tool JSONC)
endif()

install(TARGETS persist-tool RUNTIME DESTINATION bin)
