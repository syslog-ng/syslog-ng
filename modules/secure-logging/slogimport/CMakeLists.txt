
add_executable(slogimport slogimport.c)
target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	
target_link_libraries(slogimport eventlog syslog-ng syslog-ng-secure-logging)
install(TARGETS slogimport RUNTIME DESTINATION bin)
