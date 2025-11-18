/*
 * Copyright (c) 2002-2013 Balabit
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
#ifndef COMPAT_SOCKET_H_INCLUDED
#define COMPAT_SOCKET_H_INCLUDED 1

#include "compat.h"

#if defined(_WIN32)

/* keep windows.h lean, and exclude GDI (defines Arc) */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOGDI
#define NOGDI 1
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

/* Winsock first (must precede <windows.h> anywhere) */
#include <winsock2.h>
#include <ws2tcpip.h>

/* POSIX-ish bits commonly used in the tree */
#ifndef socklen_t
typedef int socklen_t;
#endif

#ifndef in_addr_t
typedef u_long in_addr_t;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* On Windows we rely on ws2tcpip’s struct sockaddr_storage.
 * Don’t provide our own fallback here to avoid redefinition.
 */

#else  /* POSIX */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#if ! SYSLOG_NG_HAVE_STRUCT_SOCKADDR_STORAGE
struct sockaddr_storage
{
  union
  {
    sa_family_t ss_family;
    struct sockaddr __sa;
    struct sockaddr_un __sun;
    struct sockaddr_in __sin;
#if SYSLOG_NG_ENABLE_IPV6
    struct sockaddr_in6 __sin6;
#endif
  };
};
#endif /* !SYSLOG_NG_HAVE_STRUCT_SOCKADDR_STORAGE */

#endif /* POSIX */

#if ! SYSLOG_NG_HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *dst);
#endif

#endif
