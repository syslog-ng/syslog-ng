timeutilsincludedir = ${pkgincludedir}/timeutils

EXTRA_DIST += lib/timeutils/CMakeLists.txt

timeutilsinclude_HEADERS =	\
  lib/timeutils/cache.h	\
  lib/timeutils/scan-timestamp.h\
  lib/timeutils/format.h	\
  lib/timeutils/misc.h	\
  lib/timeutils/names.h	\
  lib/timeutils/strptime-tz.h	\
  lib/timeutils/timeutils-internals.h	\
  lib/timeutils/unixtime.h	\
  lib/timeutils/zoneinfo.h	\
  lib/timeutils/wallclocktime.h

timeutils_sources =		\
 lib/timeutils/scan-timestamp.c	\
 lib/timeutils/cache.c		\
 lib/timeutils/format.c		\
 lib/timeutils/misc.c		\
 lib/timeutils/names.c		\
 lib/timeutils/strptime-tz.c	\
 lib/timeutils/unixtime.c	\
 lib/timeutils/zoneinfo.c	\
 lib/timeutils/wallclocktime.c

include lib/timeutils/tests/Makefile.am
