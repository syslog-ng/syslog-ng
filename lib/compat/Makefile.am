compatincludedir		= ${pkgincludedir}/compat

EXTRA_DIST += \
	lib/compat/CMakeLists.txt

compatinclude_HEADERS		= 	\
	lib/compat/compat.h		\
	lib/compat/getutent.h		\
	lib/compat/glib.h		\
	lib/compat/lfs.h		\
	lib/compat/pio.h		\
	lib/compat/socket.h		\
	lib/compat/string.h		\
	lib/compat/time.h		\
	lib/compat/openssl_support.h	\
	lib/compat/pcre.h		\
	lib/compat/getent.h		\
	lib/compat/getent-sun.h		\
	lib/compat/getent-generic.h	\
	lib/compat/un.h

compat_sources			= 	\
	lib/compat/getutent.c		\
	lib/compat/inet_aton.c		\
	lib/compat/memrchr.c		\
	lib/compat/pio.c		\
	lib/compat/glib.c		\
	lib/compat/strcasestr.c		\
	lib/compat/strtok_r.c		\
	lib/compat/strnlen.c		\
	lib/compat/time.c		\
	lib/compat/openssl_support.c	\
	lib/compat/getent-sun.c		\
	lib/compat/getent-generic.c

include lib/compat/tests/Makefile.am
