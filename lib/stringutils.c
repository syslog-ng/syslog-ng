/*
 * Copyright (c) 2015 BalaBit S.a.r.l., Luxembourg, Luxembourg
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

#include "stringutils.h"

#include <string.h>

typedef struct _StringSlice {
  char *start;
  int length;
  const char *string;
} StringSlice;

static void
_find_string(gpointer data, gpointer u_data)
{
  const char *item = (const char*) data;
  StringSlice *user_data = (StringSlice*) u_data;

  if (!user_data->start)
  {
    user_data->start = strstr(user_data->string, item);
    user_data->length = (user_data->start) ? strlen(item) : 0;
  }
}

/* searches for str in list and returns the first occurence, otherwise NULL */
guchar*
g_string_list_find_first(GList *list, const char * str, int *result_length)
{
  StringSlice user_data = {NULL, 0, str};

  g_list_foreach(list, _find_string, (gpointer) &user_data);

  *result_length = user_data.length;

  return (guchar*) user_data.start;
}

