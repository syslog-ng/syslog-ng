/*
 * Copyright (c) 2025 One Identity
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

#pragma once

#ifndef _WIN32
/* POSIX path: nothing to do */
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#else
/* Windows: minimal equivalents + stubs */
#define WIN32_LEAN_AND_MEAN 1
#define NOGDI 1
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <process.h>
#include <errno.h>
#include <stdlib.h>

/* session */
static inline int setsid(void)
{
  return 0;
}

/* rlimit stubs */
typedef unsigned long rlim_t;
struct rlimit
{
  rlim_t rlim_cur, rlim_max;
};
#ifndef RLIM_INFINITY
# define RLIM_INFINITY ((rlim_t)-1)
#endif
#ifndef RLIMIT_NOFILE
# define RLIMIT_NOFILE 7
#endif
#ifndef RLIMIT_CORE
# define RLIMIT_CORE 4
#endif
static inline int getrlimit(int, struct rlimit *rl)
{
  if (rl) rl->rlim_cur = rl->rlim_max = 16384;
  return 0;
}
static inline int setrlimit(int, const struct rlimit *)
{
  return 0;
}

/* ids/chroot (no real support on Windows) */
typedef int uid_t;
typedef int gid_t;
static inline int chroot(const char *)
{
  errno = ENOSYS;
  return -1;
}
static inline int setuid(uid_t)
{
  return 0;
}
static inline int setgid(gid_t)
{
  return 0;
}
static inline int initgroups(const char *, gid_t)
{
  return 0;
}
static inline int getuid(void)
{
  return 0;
}

/* small libc diffs */
static inline int pipe(int fds[2])
{
  return _pipe(fds, 4096, _O_BINARY);
}
static inline unsigned int sleep(unsigned int s)
{
  Sleep(s*1000u);
  return 0;
}

/* signals/waitpid/kill macros don’t exist -> keep POSIX code guarded in .c */
#endif
