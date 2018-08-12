/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balázs Scheidler
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

#include "syslog-parser.h"
#include "syslog-format.h"

static gboolean
syslog_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                      gsize input_len G_GNUC_UNUSED)
{
  SyslogParser *self = (SyslogParser *) s;
  LogMessage *msg;

  msg = log_msg_make_writable(pmsg, path_options);
  msg_trace("syslog-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_printf("msg", "%p", *pmsg));

  syslog_format_handler(&self->parse_options, (guchar *) input, strlen(input), msg);
  return TRUE;
}

static gboolean
syslog_parser_init(LogPipe *s)
{
  SyslogParser *self = (SyslogParser *) s;

  msg_format_options_init(&self->parse_options, log_pipe_get_config(s));
  return log_parser_init_method(s);
}

static LogPipe *
syslog_parser_clone(LogPipe *s)
{
  SyslogParser *self = (SyslogParser *) s;
  SyslogParser *cloned;

  cloned = (SyslogParser *) syslog_parser_new(s->cfg);
  cloned->super.template = log_template_ref(self->super.template);
  msg_format_options_copy(&cloned->parse_options, &self->parse_options);
  return &cloned->super.super;
}

static void
syslog_parser_free(LogPipe *s)
{
  SyslogParser *self = (SyslogParser *) s;

  msg_format_options_destroy(&self->parse_options);
  log_parser_free_method(s);
}

/*
 * Parse comma-separated values from a log message.
 */
LogParser *
syslog_parser_new(GlobalConfig *cfg)
{
  SyslogParser *self = g_new0(SyslogParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = syslog_parser_free;
  self->super.super.clone = syslog_parser_clone;
  self->super.super.init = syslog_parser_init;
  self->super.process = syslog_parser_process;
  msg_format_options_defaults(&self->parse_options);
  return &self->super;
}
