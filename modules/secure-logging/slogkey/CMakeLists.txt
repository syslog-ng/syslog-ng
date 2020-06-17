
add_executable(slogkey slogkey.c)
target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	
target_link_libraries(slogkey eventlog syslog-ng syslog-ng-secure-logging)
install(TARGETS slogkey RUNTIME DESTINATION bin)
