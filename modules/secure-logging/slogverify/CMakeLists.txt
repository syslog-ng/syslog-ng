
add_executable(slogverify slogverify.c)
target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	
target_link_libraries(slogverify eventlog syslog-ng secure-logging)
set_target_properties(slogverify PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
install(TARGETS slogverify RUNTIME DESTINATION bin)
