/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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

#include "tags-parser.h"
#include "scanner/list-scanner/list-scanner.h"
#include "parser/parser-expr.h"
#include "scratch-buffers.h"

#include <string.h>

typedef struct _TagsParser
{
  LogParser super;
} TagsParser;

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
         gsize input_len)
{
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  msg_debug("tags-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_printf("msg", "%p", *pmsg));

  ListScanner scanner;
  list_scanner_init(&scanner);
  list_scanner_input_va(&scanner, input, NULL);

  while (list_scanner_scan_next(&scanner))
    {
      log_msg_set_tag_by_name(msg, list_scanner_get_current_value(&scanner));
    }

  list_scanner_deinit(&scanner);
  return TRUE;
}

static LogPipe *
tags_parser_clone(LogPipe *s)
{
  TagsParser *self = (TagsParser *) s;
  TagsParser *cloned;

  cloned = (TagsParser *) tags_parser_new(s->cfg);
  cloned->super.template = log_template_ref(self->super.template);
  return &cloned->super.super;
}

/*
 * Parse comma-separated values from a log message.
 */
LogParser *
tags_parser_new(GlobalConfig *cfg)
{
  TagsParser *self = g_new0(TagsParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.clone = tags_parser_clone;
  self->super.process = _process;
  return &self->super;
}
