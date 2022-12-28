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

#include "journald-helper.h"
#include <string.h>

static void
__parse_data(gchar *data, size_t length, gchar **key, gsize *key_len, gchar **value, gsize *value_len)
{
  gchar *pos = strchr(data, '=');
  if (pos)
    {
      *key = data;
      *key_len = pos - data;
      *value = pos + 1;
      *value_len = length - (pos - data + 1);
    }
  else
    {
      *key = NULL;
      *value = NULL;
    }
}

void
journald_foreach_data(sd_journal *journal, FOREACH_DATA_CALLBACK func, gpointer user_data)
{
  const void *data;
  size_t l = 0;

  for (sd_journal_restart_data(journal); sd_journal_enumerate_data(journal, &data, &l) > 0; )
    {
      gchar *key;
      gsize key_len;
      gchar *value;
      gsize value_len;

      __parse_data((gchar *)data, l, &key, &key_len, &value, &value_len);
      if (key && value)
        {
          func(key, key_len, value, value_len, user_data);
        }
    }
}
