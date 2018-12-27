timeutilsincludedir = ${pkgincludedir}/timeutils

EXTRA_DIST += lib/timeutils/CMakeLists.txt

timeutilsinclude_HEADERS =	\
  lib/timeutils/decode.h	\
  lib/timeutils/timeutils.h	\
  lib/timeutils/zoneinfo.h

timeutils_sources =		\
 lib/timeutils/decode.c		\
 lib/timeutils/timeutils.c	\
 lib/timeutils/zoneinfo.c

include lib/timeutils/tests/Makefile.am
