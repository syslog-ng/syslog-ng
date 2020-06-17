
add_executable(slogencrypt slogencrypt.c)
target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	
target_link_libraries(slogencrypt eventlog syslog-ng syslog-ng-secure-logging)
install(TARGETS slogencrypt RUNTIME DESTINATION bin)
