
add_executable(pdbtool pdbtool.c)
target_link_libraries(pdbtool patterndb eventlog)
set_target_properties(pdbtool PROPERTIES COMPILE_FLAGS "-Wno-pointer-sign")
target_include_directories(pdbtool PRIVATE ${PROJECT_SOURCE_DIR}/modules/dbparser ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS pdbtool RUNTIME DESTINATION bin)
