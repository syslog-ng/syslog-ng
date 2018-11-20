add_library(eventlog SHARED
  src/evtctx.c
  src/evtfmt.c
  src/evtout.c
  src/evtrec.c
  src/evtstr.c
  src/evtsyslog.c
  src/evttags.c
)

target_include_directories(eventlog
  SYSTEM
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE
    ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(eventlog
  PRIVATE
    ${GLIB_LIBRARIES}
)

install(TARGETS eventlog DESTINATION lib)

add_test_subdirectory(tests)
