/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#include "map-value-pairs.h"

static gboolean
_map_name_values(const gchar *name,
                 TypeHint type, const gchar *value, gsize value_len,
                 gpointer user_data)
{
  LogMessage *msg = (LogMessage *) user_data;

  log_msg_set_value_by_name(msg, name, value, value_len);
  return FALSE;
}

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options,
         const gchar *input, gsize input_len)
{
  MapValuePairs *self = (MapValuePairs *) s;
  GlobalConfig *cfg = log_pipe_get_config(&s->super);
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  msg_trace("value-pairs message processing started",
            evt_tag_str ("input", input),
            evt_tag_printf("msg", "%p", *pmsg));

  value_pairs_foreach(self->value_pairs, _map_name_values,
                      msg,
                      0, LTZ_LOCAL, &cfg->template_options,
                      msg);

  return TRUE;
}

static LogPipe *
_clone(LogPipe *s)
{
  MapValuePairs *self = (MapValuePairs *) s;
  MapValuePairs *cloned;

  cloned = (MapValuePairs *) map_value_pairs_new(log_pipe_get_config(s), value_pairs_ref(self->value_pairs));
  return &cloned->super.super;
}

static void
_free(LogPipe *s)
{
  MapValuePairs *self = (MapValuePairs *) s;

  value_pairs_unref(self->value_pairs);
  log_parser_free_method(&self->super.super);
}

LogParser *
map_value_pairs_new(GlobalConfig *cfg, ValuePairs *value_pairs)
{
  MapValuePairs *self = g_new0(MapValuePairs, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = _free;
  self->super.super.clone = _clone;
  self->super.process = _process;
  self->value_pairs = value_pairs;
  return &self->super;
}
