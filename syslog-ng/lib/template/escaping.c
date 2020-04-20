/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#include "template/escaping.h"
#include "str-format.h"

#include <string.h>

void
result_append(GString *result, const gchar *sstr, gssize len, gboolean escape)
{
  gint i;
  const guchar *ustr = (const guchar *) sstr;

  if (len < 0)
    len = strlen(sstr);

  if (escape)
    {
      for (i = 0; i < len; i++)
        {
          if (ustr[i] == '\'' || ustr[i] == '"' || ustr[i] == '\\')
            {
              g_string_append_c(result, '\\');
              g_string_append_c(result, ustr[i]);
            }
          else if (ustr[i] < ' ')
            {
              g_string_append_c(result, '\\');
              format_uint32_padded(result, 3, '0', 8, ustr[i]);
            }
          else
            g_string_append_c(result, ustr[i]);
        }
    }
  else
    g_string_append_len(result, sstr, len);
}
