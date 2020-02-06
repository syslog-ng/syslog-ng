
add_executable(slogimport slogimport.c)
target_link_libraries(slogimport eventlog)
set_target_properties(slogimport PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
install(TARGETS slogimport RUNTIME DESTINATION bin)
