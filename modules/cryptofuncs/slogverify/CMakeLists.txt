
add_executable(slogverify slogverify.c)
target_link_libraries(slogverify eventlog)
set_target_properties(slogverify PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
install(TARGETS slogverify RUNTIME DESTINATION bin)
