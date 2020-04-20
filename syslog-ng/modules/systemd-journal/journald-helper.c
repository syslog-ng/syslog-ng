/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "journald-subsystem.h"

#include <string.h>

#define JOURNALD_FOREACH_DATA(j, data, l)                             \
        for (journald_restart_data(j); journald_enumerate_data((j), &(data), &(l)) > 0; )

static void
__parse_data(gchar *data, size_t length, gchar **key, gchar **value)
{
  gchar *pos = strchr(data, '=');
  if (pos)
    {
      guint32 max_value_len = length - (pos - data + 1);
      *key = g_strndup(data, pos - data);
      *value = g_strndup(pos + 1, max_value_len);
    }
  else
    {
      *key = NULL;
      *value = NULL;
    }
}

void journald_foreach_data(Journald *self, FOREACH_DATA_CALLBACK func, gpointer user_data)
{
  const void *data;
  size_t l = 0;

  JOURNALD_FOREACH_DATA(self, data, l)
  {
    gchar *key;
    gchar *value;

    __parse_data((gchar *)data, l, &key, &value);
    if (key && value)
      {
        func(key, value, user_data);
        g_free(key);
        g_free(value);
      }
  }
}
