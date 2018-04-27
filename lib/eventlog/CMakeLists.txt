add_library(eventlog SHARED
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtctx.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtfmt.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtout.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtrec.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtstr.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtsyslog.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evttags.c
)

target_include_directories(eventlog SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS eventlog DESTINATION lib)

add_test_subdirectory(tests)
