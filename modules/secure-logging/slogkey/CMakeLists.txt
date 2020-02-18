
add_executable(slogkey slogkey.c)
target_link_libraries(slogkey eventlog syslog-ng secure-logging)
set_target_properties(slogkey PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
install(TARGETS slogkey RUNTIME DESTINATION bin)
