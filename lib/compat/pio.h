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
#ifndef COMPAT_PIO_H_INCLUCED
#define COMPAT_PIO_H_INCLUDED 1

#include "compat.h"

#include <sys/types.h>
#include <unistd.h>

/* NOTE: bb__ prefix is used for function names that might clash with system
 * supplied symbols. */

#if !defined (SYSLOG_NG_HAVE_PREAD) || defined(SYSLOG_NG_HAVE_BROKEN_PREAD)
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

#endif
