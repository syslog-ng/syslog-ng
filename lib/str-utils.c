/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "str-utils.h"

GString *
g_string_assign_len(GString *s, const gchar *val, gint len)
{
  g_string_truncate(s, 0);
  if (val && len)
    g_string_append_len(s, val, len);
  return s;
}

void
g_string_steal(GString *s)
{
  s->str = g_malloc0(1);
  s->allocated_len = 1;
  s->len = 0;
}

static gchar *
str_replace_char(const gchar *str, const gchar from, const gchar to)
{
  gchar *p;
  gchar *ret = g_strdup(str);
  p = ret;
  while (*p)
    {
      if (*p == from)
        *p = to;
      p++;
    }
  return ret;
}

/*
  This function normalizes differently than the flags version. TODO:
  investigate if the two can be aligned. Hint: as flag normalization has
  strong position in the code, aligning block normalization to flag
  normalization might prove easier. Some more info:
  https://github.com/balabit/syslog-ng/pull/2162#discussion_r202247468
*/
gchar *
__normalize_key(const gchar *buffer)
{
  return str_replace_char(buffer, '-', '_');
}

gchar *
normalize_flag(const gchar *buffer)
{
  return str_replace_char(buffer, '_', '-');
}
