timeutilsincludedir = ${pkgincludedir}/timeutils

EXTRA_DIST += lib/timeutils/CMakeLists.txt

timeutilsinclude_HEADERS =	\
  lib/timeutils/cache.h	\
  lib/timeutils/conv.h	\
  lib/timeutils/scan-timestamp.h\
  lib/timeutils/format.h	\
  lib/timeutils/misc.h	\
  lib/timeutils/names.h	\
  lib/timeutils/unixtime.h	\
  lib/timeutils/zonecache.h	\
  lib/timeutils/zonedb.h	\
  lib/timeutils/zoneinfo.h	\
  lib/timeutils/wallclocktime.h

timeutils_sources =		\
 lib/timeutils/scan-timestamp.c	\
 lib/timeutils/cache.c		\
 lib/timeutils/conv.c		\
 lib/timeutils/format.c		\
 lib/timeutils/misc.c		\
 lib/timeutils/names.c		\
 lib/timeutils/unixtime.c	\
 lib/timeutils/zonecache.c	\
 lib/timeutils/zonedb.c		\
 lib/timeutils/zoneinfo.c	\
 lib/timeutils/wallclocktime.c

include lib/timeutils/tests/Makefile.am
