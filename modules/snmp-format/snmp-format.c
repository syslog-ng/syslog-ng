/*
 * Copyright (c) 2015 Gergely Nagy
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
 */

#include "snmp-format.h"
#include "logmsg.h"
#include "messages.h"
#include "misc.h"
#include "cfg.h"
#include "str-format.h"
#include "scratch-buffers.h"

#include <string.h>
#include <ctype.h>

static gboolean
log_msg_parse_snmp(LogMessage *msg, const gchar *input, gsize input_len)
{
  SBGString *key, *value;
  gboolean parsed = TRUE;
  gsize pos = 0;

  key = sb_gstring_acquire();
  value = sb_gstring_acquire();

  while (pos < input_len)
    {
      char *assign_pos, *value_pos, *value_end;
      gsize key_len;

      while ((input[pos] == ' ') && (pos < input_len))
        pos++;

      assign_pos = strchr(input + pos, '=');
      if (assign_pos == NULL)
        {
          parsed = FALSE;
          break;
        }

      key_len = assign_pos - input - pos;
      if (input[pos + key_len - 1] == ' ')
        key_len--;

      g_string_assign(sb_gstring_string(key), ".snmp.");
      g_string_append_len(sb_gstring_string(key), input + pos,
                          key_len);

      value_pos = strchr(assign_pos, ':');
      if (value_pos == NULL)
        {
          parsed = FALSE;
          break;
        }
      value_pos++;

      if (value_pos[0] == ' ')
        value_pos++;

      if (value_pos[0] == '"')
        {
          value_pos++;
          value_end = strchr(value_pos, '"');
          pos = value_end - input + 1;
        }
      else
        {
          value_end = strchr(value_pos, ' ');
          if (!value_end)
            value_end = strchr(value_pos, '\0');
          if (!value_end)
            {
              parsed = FALSE;
              break;
            }
          pos = value_end - input;
        }

      g_string_assign(sb_gstring_string(value),
                      value_pos);
      g_string_truncate(sb_gstring_string(value),
                        value_end - value_pos);

      log_msg_set_value_by_name(msg,
                                sb_gstring_string(key)->str,
                                sb_gstring_string(value)->str,
                                sb_gstring_string(value)->len);
    }

  sb_gstring_release(key);
  sb_gstring_release(value);

  return parsed;
}

void
snmp_format_handler(const MsgFormatOptions *parse_options,
                    const guchar *data, gsize length,
                    LogMessage *self)
{
  gboolean success;

  if (parse_options->flags & LP_NOPARSE)
    {
      log_msg_set_value(self, LM_V_MESSAGE, (gchar *) data, length);
      self->pri = parse_options->default_pri;
      return;
    }

  self->flags |= LF_UTF8;

  if (parse_options->flags & LP_LOCAL)
    self->flags |= LF_LOCAL;

  self->initial_parse = TRUE;

  success = log_msg_parse_snmp(self, (gchar *)data, length);

  self->initial_parse = FALSE;

  if (G_UNLIKELY(!success))
    {
      msg_format_inject_parse_error(self, data, length);
      return;
    }
}
