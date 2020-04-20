/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <string.h>
#include <ctype.h>

static void
tf_basename(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gchar *base;

  base = g_path_get_basename(argv[0]->str);
  g_string_append(result, base);
  g_free(base);
}

TEMPLATE_FUNCTION_SIMPLE(tf_basename);

static void
tf_dirname(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gchar *dir;

  dir = g_path_get_dirname(argv[0]->str);
  g_string_append(result, dir);
  g_free(dir);
}

TEMPLATE_FUNCTION_SIMPLE(tf_dirname);
