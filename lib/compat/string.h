/*
 * Copyright (c) 2002-2013 Balabit
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
#ifndef COMPAT_STRING_H_INCLUDED
#define COMPAT_STRING_H_INCLUDED

#include "compat.h"
#include <string.h>

#include <stddef.h>

#if !SYSLOG_NG_HAVE_STRTOLL
# if SYSLOG_NG_HAVE_STRTOIMAX || defined(strtoimax)
/* HP-UX has an strtoimax macro, not a function */
#define strtoll(nptr, endptr, base) strtoimax(nptr, endptr, base)
# else
/* this requires Glib 2.12 */
#define strtoll(nptr, endptr, base) g_ascii_strtoll(nptr, endptr, base)
# endif
#endif

#if !SYSLOG_NG_HAVE_STRCASESTR
char *strcasestr(const char *s, const char *find);
#endif

#if !SYSLOG_NG_HAVE_MEMRCHR
void *memrchr(const void *s, int c, size_t n);
#endif

#ifndef SYSLOG_NG_HAVE_STRTOK_R
char *strtok_r(char *string, const char *delim, char **saveptr);
#endif

#if !SYSLOG_NG_HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen);
#endif
#endif
