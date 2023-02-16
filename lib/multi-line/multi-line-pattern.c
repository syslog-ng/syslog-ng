/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balazs Scheidler <bazsi@balabit.hu>
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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
 */
#include "multi-line/multi-line-pattern.h"

MultiLinePattern *
multi_line_pattern_compile(const gchar *regexp, GError **error)
{
  MultiLinePattern *self = g_new0(MultiLinePattern, 1);
  gint optflags = 0;
  gint rc;
  const gchar *errptr;
  gint erroffset;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  self->ref_cnt = 1;

  /* compile the regexp */
  self->pattern = pcre_compile2(regexp, 0, &rc, &errptr, &erroffset, NULL);
  if (!self->pattern)
    {
      g_set_error(error, 0, 0, "Error while compiling multi-line regexp as a PCRE expression, error=%s, error_at=%d", errptr,
                  erroffset);
      goto error;
    }

#ifdef PCRE_STUDY_JIT_COMPILE
  optflags = PCRE_STUDY_JIT_COMPILE;
#endif

  /* optimize regexp */
  self->extra = pcre_study(self->pattern, optflags, &errptr);
  if (errptr != NULL)
    {
      g_set_error(error, 0, 0, "Error while studying multi-line regexp, error=%s", errptr);
      goto error;
    }

  return self;
error:
  if (self->pattern)
    pcre_free(self->pattern);
  g_free(self);
  return NULL;
}

gint
multi_line_pattern_find(MultiLinePattern *re, const guchar *str, gsize len, gint *matches, gint matches_num)
{
  gint rc;

  if (!re)
    return -1;

  rc = pcre_exec(re->pattern, re->extra, (const gchar *) str, len, 0, 0, matches, matches_num * 3);
  return rc;
}

gboolean
multi_line_pattern_match(MultiLinePattern *re, const guchar *str, gsize len)
{
  gint match[3];
  if (multi_line_pattern_find(re, str, len, match, 1) < 0)
    return FALSE;
  return match[0] >= 0;
}


MultiLinePattern *
multi_line_pattern_ref(MultiLinePattern *self)
{
  if (self)
    self->ref_cnt++;
  return self;
}

void
multi_line_pattern_unref(MultiLinePattern *self)
{
  if (self && (--self->ref_cnt == 0))
    {
      if (self->pattern)
        pcre_free(self->pattern);
      if (self->extra)
        pcre_free_study(self->extra);
      g_free(self);
    }
}
