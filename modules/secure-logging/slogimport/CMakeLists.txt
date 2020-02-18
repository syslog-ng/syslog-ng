
add_executable(slogimport slogimport.c)
target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	
target_link_libraries(slogimport eventlog syslog-ng secure-logging)
set_target_properties(slogimport PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
install(TARGETS slogimport RUNTIME DESTINATION bin)
