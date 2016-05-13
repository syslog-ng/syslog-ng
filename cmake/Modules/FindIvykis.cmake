include(LibFindMacros)

libfind_pkg_check_modules(Ivykis_PKGCONF ivykis)

libfind_pkg_detect(Ivykis ivykis FIND_PATH iv.h FIND_LIBRARY ivykis)
set(Ivykis_PROCESS_INCLUDES Ivykis_INCLUDE_DIR)
set(Ivykis_PROCESS_LIBS Ivykis_LIBRARY)
libfind_process(Ivykis)
