/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef COMPAT_H_INCLUDED
#define COMPAT_H_INCLUDED

#include <config.h>
#include <sys/types.h>
#include <unistd.h>

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


#endif
