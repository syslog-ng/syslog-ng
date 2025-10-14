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
#if !defined(_WIN32)
  #include <glob.h>
#else
  #include <stddef.h>

  typedef struct {
    size_t gl_pathc;   /* # of matched paths */
    char **gl_pathv;   /* vector of paths (NULL-terminated not required here) */
    size_t gl_offs;    /* ignored by our shim */
  } glob_t;

  /* flags we (partially) support */
  #define GLOB_ERR      0x01
  #define GLOB_MARK     0x02   /* append '/' to directories */
  #define GLOB_NOSORT   0x04   /* don't sort */
  #define GLOB_NOCHECK  0x08   /* if no match, return pattern itself */
  #define GLOB_APPEND   0x10   /* append to existing gl_pathv (basic support) */
  #define GLOB_DOOFFS   0x20   /* ignored */

  /* return codes */
  #define GLOB_NOSPACE  1
  #define GLOB_ABORTED  2
  #define GLOB_NOMATCH  3

  int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob);
  void globfree(glob_t *pglob);
#endif
