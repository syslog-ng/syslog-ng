/*
 * Copyright (c) 2023 Balázs Scheidler <bazsi77@gmail.com>
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

#include "sdata-parser.h"
#include "syslog-format.h"


static gboolean
sdata_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                     gsize input_len)
{
  SDataParser *self = (SDataParser *) s;
  LogMessage *msg;

  msg = log_msg_make_writable(pmsg, path_options);
  msg_trace("sdata-parser() message processing started",
            evt_tag_str("input", input),
            evt_tag_msg_reference(*pmsg));

  const guchar *data = (const guchar *) input;
  gint data_len = input_len;
  return _syslog_format_parse_sd(msg, &data, &data_len, &self->parse_options);
}

static gboolean
sdata_parser_init(LogPipe *s)
{
  SDataParser *self = (SDataParser *) s;

  msg_format_options_init(&self->parse_options, log_pipe_get_config(s));
  return log_parser_init_method(s);
}

static LogPipe *
sdata_parser_clone(LogPipe *s)
{
  SDataParser *self = (SDataParser *) s;
  SDataParser *cloned;

  cloned = (SDataParser *) sdata_parser_new(s->cfg);
  log_parser_clone_settings(&self->super, &cloned->super);
  msg_format_options_copy(&cloned->parse_options, &self->parse_options);
  return &cloned->super.super;
}

static void
sdata_parser_free(LogPipe *s)
{
  SDataParser *self = (SDataParser *) s;

  msg_format_options_destroy(&self->parse_options);
  log_parser_free_method(s);
}


LogParser *
sdata_parser_new(GlobalConfig *cfg)
{
  SDataParser *self = g_new0(SDataParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = sdata_parser_free;
  self->super.super.clone = sdata_parser_clone;
  self->super.super.init = sdata_parser_init;
  self->super.process = sdata_parser_process;
  msg_format_options_defaults(&self->parse_options);
  return &self->super;
}
