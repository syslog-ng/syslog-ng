lib_LTLIBRARIES += lib/secret-storage/libsecret-storage.la

lib_secret_storageincludedir = $(pkgincludedir)

lib_secret_storageinclude_HEADERS =     \
  lib/secret-storage/secret-storage.h

lib_secret_storage_libsecret_storage_la_SOURCES =             \
  lib/secret-storage/secret-storage.c

lib_secret_storage_libsecret_storage_la_CFLAGS = $(AM_CFLAGS)

include lib/secret-storage/tests/Makefile.am
