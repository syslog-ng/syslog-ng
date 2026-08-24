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
)

target_link_libraries(eventlog
  PRIVATE
    GLib::GLib
)

# mirrors autotools: -release major.minor -version-info 0:0:0
set_target_properties(eventlog PROPERTIES
    OUTPUT_NAME "evtlog-${LSNG_RELEASE}"
    VERSION "0.0.0"
    SOVERSION "0")

install(TARGETS eventlog DESTINATION lib)

add_test_subdirectory(tests)
