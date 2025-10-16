/*
 * Copyright (c) 2002-2018 Balabit
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
#ifndef COMPAT_UN_H_INCLUDED
#define COMPAT_UN_H_INCLUDED 1

#if defined(_WIN32)

  /* Make AF_UNIX visible (Vista+) and ensure include order */
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #ifndef WINVER
    #define WINVER 0x0600
  #endif

  /* winsock2 must come before afunix/windows.h */
  #ifndef _WINSOCKAPI_
    #include <winsock2.h>
  #endif

  /* Try the official Windows header (MSYS2 MinGW provides this) */
  #if __has_include(<afunix.h>)
    #include <afunix.h>
  #endif

  #ifndef SUN_LEN
    #include <string.h>
    #define SUN_LEN(ptr) ((size_t)(((struct sockaddr_un *)0)->sun_path) + strlen((ptr)->sun_path))
  #endif

#else /* POSIX */

#include <sys/un.h>

/*
  SUN_LEN is not a POSIX standard, thus not available on all platforms.
  If it is available we should rely on it. Otherwise we use the formula
  from the Linux man page.
*/
#ifndef SUN_LEN
#define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path) + strlen ((ptr)->sun_path))
#endif

#endif /* SYS_CHECK */

#endif /* COMPAT_UN_H_INCLUDED */
