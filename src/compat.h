#ifndef COMPAT_H_INCLUDED
#define COMPAT_H_INCLUDED

#include "syslog-ng.h"
#include <sys/types.h>

#if !HAVE_PREAD || HAVE_BROKEN_PREAD
#define pread __pread
#define pwrite __pwrite

ssize_t __pread(int fd, void *buf, size_t count, off_t offset);
ssize_t __pwrite(int fd, const void *buf, size_t count, off_t offset);

#endif

#if !HAVE_STRTOLL
# if HAVE_STRTOIMAX || defined(strtoimax)
   /* HP-UX has an strtoimax macro, not a function */
   #define strtoll(nptr, endptr, base) strtoimax(nptr, endptr, base)
# else
   /* this requires Glib 2.12 */
   #define strtoll(nptr, endptr, base) g_ascii_strtoll(nptr, endptr, base)
# endif
#endif

#if !HAVE_O_LARGEFILE
#define O_LARGEFILE 0
#endif

#if !HAVE_STRCASESTR
char *strcasestr(const char *s, const char *find);
#endif


#endif
