include(LibFindMacros)

libfind_pkg_check_modules(Eventlog_PKGCONF eventlog)

libfind_pkg_detect(Eventlog eventlog FIND_PATH evtlog.h PATH_SUFFIXES eventlog FIND_LIBRARY evtlog)
set(Eventlog_PROCESS_INCLUDES Eventlog_INCLUDE_DIR)
set(Eventlog_PROCESS_LIBS Eventlog_LIBRARY)
libfind_process(Eventlog)
