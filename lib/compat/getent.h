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

#ifndef COMPAT_GETENT_H_INCLUDED
#define COMPAT_GETENT_H_INCLUDED

#include "compat/compat.h"

#ifndef SYSLOG_NG_HAVE_GETPROTOBYNUMBER_R

#define getprotobynumber_r _compat_generic__getprotobynumber_r
#define getprotobyname_r _compat_generic__getprotobyname_r
#define getservbyport_r _compat_generic__getservbyport_r
#define getservbyname_r _compat_generic__getservbyname_r

#include "getent-generic.h"

#elif defined(sun) || defined(__sun)

#define getprotobynumber_r _compat_sun__getprotobynumber_r
#define getprotobyname_r _compat_sun__getprotobyname_r
#define getservbyport_r _compat_sun__getservbyport_r
#define getservbyname_r _compat_sun__getservbyname_r

#include "getent-sun.h"

#endif
#endif
