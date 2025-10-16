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

#if defined(_WIN32)
#include "glob.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);
  if (p) memcpy(p, s, n);
  return p;
}

static void slash_to_backslash(char *s)
{
  for (; *s; ++s) if (*s == '/') *s = '\\';
}
static void backslash_to_slash(char *s)
{
  for (; *s; ++s) if (*s == '\\') *s = '/';
}

static int push_path(glob_t *g, const char *path)
{
  size_t newc = g->gl_pathc + 1;
  char **nv = (char **)realloc(g->gl_pathv, newc * sizeof(char *));
  if (!nv) return 0;
  g->gl_pathv = nv;
  g->gl_pathv[g->gl_pathc++] = dupstr(path);
  return g->gl_pathv[g->gl_pathc-1] != NULL;
}

static int cmp_str(const void *a, const void *b)
{
  const char *sa = *(const char *const *)a;
  const char *sb = *(const char *const *)b;
  return strcmp(sa, sb);
}

int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
  (void)errfunc;
  if (!pglob) return GLOB_ABORTED;

  if (!(flags & GLOB_APPEND))
    {
      pglob->gl_pathc = 0;
      pglob->gl_pathv = NULL;
      pglob->gl_offs  = 0;
    }

  /* Make a WinAPI-friendly copy of the pattern */
  char *pat_bs = dupstr(pattern);
  if (!pat_bs) return GLOB_NOSPACE;
  slash_to_backslash(pat_bs);

  /* Split dir + mask */
  const char *last = strrchr(pat_bs, '\\');
  size_t dlen = last ? (size_t)(last - pat_bs + 1) : 0;

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pat_bs, &fd);
  if (h == INVALID_HANDLE_VALUE)
    {
      free(pat_bs);
      if (flags & GLOB_NOCHECK)
        {
          /* Return the pattern itself */
          if (!push_path(pglob, pattern)) return GLOB_NOSPACE;
          return 0;
        }
      return GLOB_NOMATCH;
    }

  int rc = 0;
  do
    {
      /* Skip '.' and '..' */
      if (fd.cFileName[0] == '.' &&
          (fd.cFileName[1] == '\0' || (fd.cFileName[1]=='.' && fd.cFileName[2]=='\0')))
        continue;

      /* Build full path using dir prefix + file name (use forward slashes for consistency) */
      size_t plen = dlen + strlen(fd.cFileName) + 2;
      char *full = (char *)malloc(plen);
      if (!full)
        {
          rc = GLOB_NOSPACE;
          break;
        }

      if (dlen)
        {
          memcpy(full, pat_bs, dlen);
          strcpy(full + dlen, fd.cFileName);
        }
      else
        {
          strcpy(full, fd.cFileName);
        }
      backslash_to_slash(full);

      if ((flags & GLOB_MARK) && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
          size_t L = strlen(full);
          if (L == 0 || full[L-1] != '/')
            full = (char *)realloc(full, L + 2), strcpy(full + L, "/");
        }

      if (!push_path(pglob, full))
        {
          free(full);
          rc = GLOB_NOSPACE;
          break;
        }
      free(full);
    }
  while (FindNextFileA(h, &fd));

  FindClose(h);
  free(pat_bs);

  if (rc != 0) return rc;

  if (pglob->gl_pathc == 0)
    {
      if (flags & GLOB_NOCHECK)
        {
          if (!push_path(pglob, pattern)) return GLOB_NOSPACE;
          return 0;
        }
      return GLOB_NOMATCH;
    }

  if (!(flags & GLOB_NOSORT) && pglob->gl_pathc > 1)
    qsort(pglob->gl_pathv, pglob->gl_pathc, sizeof(char *), cmp_str);

  return 0;
}

void globfree(glob_t *pglob)
{
  if (!pglob) return;
  for (size_t i = 0; i < pglob->gl_pathc; ++i) free(pglob->gl_pathv[i]);
  free(pglob->gl_pathv);
  pglob->gl_pathv = NULL;
  pglob->gl_pathc = 0;
}
#endif
