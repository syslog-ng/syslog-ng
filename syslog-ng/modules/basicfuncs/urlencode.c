/*
 * Copyright (c) 2018 Balabit
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

static void
tf_urlencode(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  if (argc < 1)
    return;

  for (gint i = 0; i < argc; i++)
    {
      gchar *escaped = g_uri_escape_string(argv[i]->str, NULL, FALSE);
      g_string_append(result, escaped);
      g_free(escaped);
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_urlencode);

static void
tf_urldecode(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  if (argc < 1)
    return;

  for (gint i = 0; i < argc; i++)
    {
      gchar *escaped = g_uri_unescape_string(argv[i]->str, NULL);
      if (escaped)
        {
          g_string_append(result, escaped);
          g_free(escaped);
        }
      else
        {
          msg_error("Could not urldecode", evt_tag_str("str", argv[i]->str));
        }
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_urldecode);
