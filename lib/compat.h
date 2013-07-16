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
#ifdef _WIN32
/* windows includes above all ... */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <iv_list.h>
#include <iv_event_raw.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#include <evtlog.h>

int getsockerror();
EVTTAG *evt_tag_socket_error(const char *name, int value);

#ifdef _WIN32

/* ivykis internal stuff, needed by logmsg. Can't include the iv_list.h because
 * VS 2k5 croaks on it... */

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define INIT_LIST_HEAD(lh) do { \
    (lh)->next = (lh); \
    (lh)->prev = (lh); \
} while (0)

#define S_IFMT  00170000
#define S_IFSOCK 0140000

#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)

#define SHUT_RDWR SD_BOTH
#define getpid GetCurrentProcessId

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef sleep
#define sleep(x) Sleep(x * 1000)
#endif

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
long getpagesize (void);
#if (_WIN32_WINNT < 0x0600)
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);
int inet_pton(int af, const char *src, void *dst);
#endif /*_WIN32_WINNT >= 0x0600*/
int inet_aton(const char *cp, struct in_addr *dst);

#include "pthread.h"
#else /* _WIN32 */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mman.h>
#endif /* _WIN32 */

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

#ifdef _WIN32
#ifdef __MINGW32__
#include <sys/types.h>
#endif

#ifndef ESTALE
#define ESTALE WSAESTALE
#endif

#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK  WSAEWOULDBLOCK
#endif

#ifndef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#endif

#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif

int fsync (int fd);
int nanosleep(const struct timespec *request, struct timespec *remain);

int kill(pid_t pid, int sig);

int res_init();

#define WNOHANG 1
#define SIGHUP 1
#define SIGCHLD 1
#ifndef SIGTERM
#define SIGTERM 1
#endif
#ifndef SIGINT
#define SIGINT 1
#endif
#define SIGALRM         14      /* Alarm clock (POSIX).  */

typedef unsigned int uid_t;
typedef unsigned int gid_t;

pid_t waitpid(pid_t pid, int *status, int options);



int alarm(unsigned int seconds);

struct sigaction{
	int dummy;
	void (*sa_handler) (int signo);
};

struct iv_signal {
	int			signum;
	unsigned int flags;
	void			*cookie;
	void			(*handler)(void *);

	struct iv_list_head	list;
	struct iv_event_raw	ev;
	int			active;
};

#define IV_SIGNAL_FLAG_EXCLUSIVE  1

void IV_SIGNAL_INIT(struct iv_signal *this);

int iv_signal_register(struct iv_signal *this);
void iv_signal_unregister(struct iv_signal *this);

void syslog(int pri, const char *bufp, ...);
void openlog(const char *ident, int option, int facility);

int chown(const char *path, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int fchmod(int fd, mode_t mode);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);


#define LOG_PID         0x01    /* log the pid with each message */
#define LOG_CONS        0x02    /* log on the console if errors in sending */
#define LOG_ODELAY      0x04    /* delay open until first syslog() (default) */
#define LOG_NDELAY      0x08    /* don't delay open */
#define LOG_NOWAIT      0x10    /* don't wait for console forks: DEPRECATED */
#define LOG_PERROR      0x20    /* log to stderr as well */


#define INITIALIZER(f) \
   static void f(void) __attribute__((constructor));\
   static void f(void)

#define CLOCK_REALTIME 1
int clock_gettime(int clock_id, struct timespec *res);

#elif defined(__GNUC__) /* _WIN32 */

#define CCALL
#define INITIALIZER(f) \
   static void f(void) __attribute__((constructor));\
   static void f(void)

#define WSAEINPROGRESS EINPROGRESS

#else /* _WIN32 */
#define BB_CRYPTO_EXPORT
#define BB_CRYPTO_IMPORT

#endif /* _WIN32 */

void init_signals();

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timer, struct tm *result);
#endif

#ifdef _WIN32
struct iv_fd {
  SOCKET  fd;
  void    *cookie;
  void    (*handler[FD_MAX_EVENTS])(void *, int, int);
  struct iv_handle handle;
  void    (*handler_in)(void *cookie);
  void    (*handler_out)(void *cookie);
  void    (*handler_err)(void *cookie);
  long    event_mask;
};

void IV_FD_INIT(struct iv_fd *);
void iv_fd_register(struct iv_fd *);
int iv_fd_register_try(struct iv_fd *);
void iv_fd_unregister(struct iv_fd *);
int iv_fd_registered(struct iv_fd *);
void iv_fd_set_handler_in(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_out(struct iv_fd *, void (*)(void *));
void iv_fd_set_handler_err(struct iv_fd *, void (*)(void *));

#endif

#endif /* COMPAT_H_INCLUDED */
