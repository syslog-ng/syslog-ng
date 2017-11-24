
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/eventlog/src/include)
add_library(eventlog SHARED
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtctx.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtfmt.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtout.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtrec.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtstr.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evtsyslog.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/evttags.c
)

install(TARGETS eventlog DESTINATION lib)
