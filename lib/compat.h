/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef COMPAT_H_INCLUDED
#define COMPAT_H_INCLUDED

#include <config.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#ifdef _MSC_VER

/* ivykis internal stuff, needed by logmsg. Can't include the iv_list.h because
 * VS 2k5 croaks on it... */
struct list_head {
	struct list_head	*next;
	struct list_head	*prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define INIT_LIST_HEAD(lh) do { \
    (lh)->next = (lh); \
    (lh)->prev = (lh); \
} while (0)

#define STRICT
#include <windows.h>
#undef STRICT
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef ssize_t
typedef signed int ssize_t;
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef sleep
#define sleep(x) Sleep(x * 1000)
#endif

typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int mode_t;
typedef unsigned int pid_t;

#define SIGQUIT 3

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#define LOG_KERN    (0<<3)
#define LOG_USER    (1<<3)
#define LOG_MAIL    (2<<3)
#define LOG_DAEMON  (3<<3)
#define LOG_AUTH    (4<<3)
#define LOG_SYSLOG  (5<<3)
#define LOG_LPR     (6<<3)
#define LOG_NEWS    (7<<3)
#define LOG_UUCP    (8<<3)
#define LOG_CRON    (9<<3)
#define LOG_AUTHPRIV    (10<<3)
#define LOG_FTP     (11<<3)

struct iovec
{
  void*   iov_base;
  size_t  iov_len;
};

char *strptime(const char *buf, const char *fmt, struct tm *tm);

ssize_t writev(int fd, const struct iovec *vector, int count);
ssize_t readv(int fd, const struct iovec *vector, int count);

long getpagesize(void);
/*
 * Protections are chosen from these bits, or-ed together
 */
#define	PROT_NONE	0x00	/* [MC2] no permissions */
#define	PROT_READ	0x01	/* [MC2] pages can be read */
#define	PROT_WRITE	0x02	/* [MC2] pages can be written */
#define	PROT_EXEC	0x04	/* [MC2] pages can be executed */

#define	MAP_FIXED	 0x0010	/* [MF|SHM] interpret addr exactly */
/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define	MAP_SHARED	0x0001		/* [MF|SHM] share changes */
#define	MAP_PRIVATE	0x0002		/* [MF|SHM] changes are private */

/*
 * Error return from mmap()
 */
#define MAP_FAILED	((void *)-1)	/* [MF|SHM] mmap failed */


void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int munmap(void *addr, size_t len);
int madvise(void *addr, size_t len, int advice);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);
int inet_pton(int af, const char *src, void *dst);
int inet_aton(const char *cp, struct in_addr *dst);

/* Timespec declaration for systems lacking one. */
struct timespec {
    time_t   tv_sec;        /* seconds */
    long int    tv_nsec;       /* nanoseconds */
};
#else /* _MSC_VER */
#include <sys/socket.h>
#endif /* _MSC_VER */

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* NOTE: bb__ prefix is used for function names that might clash with system
 * supplied symbols. */

#if !HAVE_PREAD || HAVE_BROKEN_PREAD
# ifdef pread
#  undef pread
# endif
# ifdef pwrite
#  undef pwrite
# endif
#define pread bb__pread
#define pwrite bb__pwrite

ssize_t bb__pread(int fd, void *buf, size_t count, off_t offset);
ssize_t bb__pwrite(int fd, const void *buf, size_t count, off_t offset);

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

#if !HAVE_MEMRCHR
const void *memrchr(const void *s, int c, size_t n);
#endif

/* On HP-UX, the standard functions for sockets don't use socklen_t(which is
 * size_t, which is a long int) for optlen, just a simple int ...
 * Name resolving functions and the like use socklen_t though.
 * Please use the bb_socklen_t for socket handling functions.
 */
#if defined(__hpux__) || defined(__osf__)
typedef int bb_socklen_t;
#else
typedef socklen_t bb_socklen_t;
#endif

#ifdef _MSC_VER

#define CCALL __cdecl
#pragma section(".CRT$XCU",read)
#define INITIALIZER(f) \
   static void __cdecl f(void); \
   __declspec(allocate(".CRT$XCU")) void (__cdecl*f##_)(void) = f; \
   static void __cdecl f(void)

#elif defined(__GNUC__)

#define CCALL
#define INITIALIZER(f) \
   static void f(void) __attribute__((constructor));\
   static void f(void)

#endif

/* FIXME: Ugly hack to force loading and linking crypto.c on Windows. Without it, the linker skips the module... */
#ifdef _MSC_VER
#define BB_CRYPTO_EXPORT void crypto_init_vs(void) { return; }
#define BB_CRYPTO_IMPORT crypto_init_vs();
void crypto_init_vs(void);

#else
#define BB_CRYPTO_EXPORT
#define BB_CRYPTO_IMPORT
#endif

#endif
