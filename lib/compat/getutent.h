/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#ifndef COMPAT_GETUTENT_H_INCLUDED
#define COMPAT_GETUTENT_H_INCLUDED

#include "compat/compat.h"
#include <inttypes.h>
#include <time.h>

#ifdef _WIN32
/* Windows: no utmp/utmpx — provide minimal stubs */
typedef struct
{
  int _unused;
} utmp;

static inline struct utmp *getutent(void)
{
  return NULL;
}
static inline void endutent(void)
{
  return;
}

#else  /* POSIX */

#if SYSLOG_NG_HAVE_UTMPX_H
#include <utmpx.h>
#else
#include <utmp.h>
#endif

#if ! SYSLOG_NG_HAVE_GETUTENT && ! SYSLOG_NG_HAVE_GETUTXENT
struct utmp *getutent(void);
void endutent(void);
#endif

#endif /* _WIN32 */

#endif /* COMPAT_GETUTENT_H_INCLUDED */
