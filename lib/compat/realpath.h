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

/* POSIX: just use the real one */
#include <stdlib.h>
#define compat_realpath realpath

#else

/* Windows: emulate POSIX realpath() via _fullpath() */
#include <stdlib.h>
#include <string.h>

/* Behaves like POSIX realpath():
 * - If resolved_path == NULL, returns a newly malloc'ed absolute path.
 * - If resolved_path != NULL, writes into it and returns resolved_path.
 *   (Caller must ensure the buffer is large enough, e.g. PATH_MAX.)
 * On failure, returns NULL and sets errno.
 */
static inline char *compat_realpath(const char *path, char *resolved_path)
{
  /* Let CRT allocate if caller passed NULL (mirrors POSIX malloc behavior). */
  char *tmp = _fullpath(NULL, path, 0);
  if (!tmp)
    return NULL;

  if (resolved_path)
    {
      /* Caller promised enough space; copy and free temp. */
      strcpy(resolved_path, tmp);
      free(tmp);
      return resolved_path;
    }

  /* Caller wanted an allocated buffer; hand tmp back. */
  return tmp;
}

#endif /* _WIN32 */