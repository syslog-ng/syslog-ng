/*
 * Copyright (c) 2017 Balabit
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

#ifndef GETENT_BB_H_INCLUDED
#define GETENT_BB_H_INCLUDED

#if defined(sun) || defined(__sun)

#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <netdb.h>

int bb__getprotobynumber_r(int proto,
                           struct protoent *result_buf, char *buf,
                           size_t buflen, struct protoent **result);

int bb__getprotobyname_r(const char *name,
                         struct protoent *result_buf, char *buf,
                         size_t buflen, struct protoent **result);

int bb__getservbyport_r(int port, const char *proto,
                        struct servent *result_buf, char *buf,
                        size_t buflen, struct servent **result);

int bb__getservbyname_r(const char *name, const char *proto,
                        struct servent *result_buf, char *buf,
                        size_t buflen, struct servent **result);

#endif

#endif
