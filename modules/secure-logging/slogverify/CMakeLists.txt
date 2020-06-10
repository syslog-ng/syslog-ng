
add_executable(slogverify slogverify.c)
target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	
target_link_libraries(slogverify eventlog syslog-ng syslog-ng-secure-logging)
install(TARGETS slogverify RUNTIME DESTINATION bin)
