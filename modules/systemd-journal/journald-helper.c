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
#include "messages.h"
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

#define BOOT_ID_BUFF_LEN 33
#define MATCH_BUFF_LEN 64

int
journald_filter_this_boot(sd_journal *journal)
{
  char match[MATCH_BUFF_LEN + 1] = {0};

  sd_id128_t boot_id;
  int ret = sd_id128_get_boot(&boot_id);
  if (ret != 0)
    {
      msg_error("systemd-journal: Failed read boot_id",
                evt_tag_errno("sd_id128_get_boot", -ret));
      return ret;
    }

  char boot_id_string[BOOT_ID_BUFF_LEN];
  sd_id128_to_string(boot_id, boot_id_string);

  g_snprintf(match, sizeof(match), "_BOOT_ID=%s", boot_id_string);
  msg_debug("systemd-journal: filtering journal to the current boot",
            evt_tag_str("match", match));

  return sd_journal_add_match(journal, match, 0);
}
