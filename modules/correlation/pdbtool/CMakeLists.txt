
add_executable(pdbtool pdbtool.c)
target_link_libraries(pdbtool patterndb eventlog)
set_target_properties(pdbtool PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
install(TARGETS pdbtool RUNTIME DESTINATION bin)
