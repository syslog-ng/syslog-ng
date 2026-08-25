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

# NAMELINK_SKIP suppresses libevtlog-${LSNG_RELEASE}.so; the unversioned linker
# symlink is added manually to match autotools (-release produces libevtlog.so).
install(TARGETS eventlog LIBRARY DESTINATION lib NAMELINK_SKIP)
install(CODE "file(CREATE_LINK \"libevtlog-${LSNG_RELEASE}.so.0.0.0\"
    \"\${CMAKE_INSTALL_PREFIX}/lib/libevtlog.so\" SYMBOLIC)")

add_test_subdirectory(tests)
