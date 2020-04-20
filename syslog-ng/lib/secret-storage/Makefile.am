lib_LTLIBRARIES += lib/secret-storage/libsecret-storage.la

EXTRA_DIST += lib/secret-storage/CMakeLists.txt

lib_secret_storageincludedir = $(pkgincludedir)

lib_secret_storageinclude_HEADERS =     \
  lib/secret-storage/secret-storage.h	\
  lib/secret-storage/nondumpable-allocator.h

lib_secret_storage_libsecret_storage_la_SOURCES =             \
  lib/secret-storage/secret-storage.c	\
  lib/secret-storage/nondumpable-allocator.c

lib_secret_storage_libsecret_storage_la_CFLAGS = $(AM_CFLAGS) -fvisibility=hidden
lib_secret_storage_libsecret_storage_la_LIBADD = @GLIB_LIBS@

include lib/secret-storage/tests/Makefile.am
