/*
 * Copyright (c) 2022 One Identity
 * Copyright (c) 2022 Kokan
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

#include "flip-parser.h"
#include "parser/parser-expr.h"
#include "scratch-buffers.h"
#include "string-list.h"

#include <string.h>

typedef struct _FlipParser
{
  LogParser super;
  gboolean reverse;
  gboolean flip;
} FlipParser;


static gboolean
_flip_and_reverse(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                  gsize input_len)
{
  log_msg_make_writable(pmsg, path_options);

  return TRUE;
}

static gboolean
_flip(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
      gsize input_len)
{
  log_msg_make_writable(pmsg, path_options);

  return TRUE;
}

static gboolean
_reverse(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
         gsize input_len)
{
  log_msg_make_writable(pmsg, path_options);

  gssize len = 0;
  const gchar *message = log_msg_get_value(*pmsg, LM_V_MESSAGE, &len);

  gchar *egassem = g_strreverse((gchar *)message);

  log_msg_set_value(*pmsg, LM_V_MESSAGE, egassem, len);


  return TRUE;
}

static gboolean
_skip(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
      gsize input_len)
{
  return TRUE;
}

static gboolean
flip_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                    gsize input_len)
{
  FlipParser *self = (FlipParser *) s;

  log_msg_make_writable(pmsg, path_options);

  if (self->flip && self->reverse)
    {
      self->super.process = _flip_and_reverse;

      return _flip_and_reverse(s, pmsg, path_options, input, input_len);
    }
  else if (self->flip)
    {
      self->super.process = _flip;

      return _flip(s, pmsg, path_options, input, input_len);
    }
  else if (self->reverse)
    {
      self->super.process = _reverse;

      return _reverse(s, pmsg, path_options, input, input_len);
    }

  self->super.process = _skip;

  return _skip(s, pmsg, path_options, input, input_len);
}


static LogPipe *
flip_parser_clone(LogPipe *s)
{
  FlipParser *self = (FlipParser *)s;
  FlipParser *cloned;

  cloned = (FlipParser *)flip_parser_new(s->cfg);
  flip_parser_set_reverse_text(&cloned->super, self->reverse);
  flip_parser_set_flip_text(&cloned->super, self->flip);

  return &cloned->super.super;
}

void
flip_parser_set_reverse_text(LogParser *s, gboolean reverse)
{
  FlipParser *self = (FlipParser *)s;

  self->reverse = reverse;
}

void
flip_parser_set_flip_text(LogParser *s, gboolean flip)
{
  FlipParser *self = (FlipParser *)s;

  self->flip = flip;
}


gboolean
flip_parser_init(LogPipe *s)
{
  FlipParser *self = (FlipParser *)s;

  if (!self->flip && !self->reverse)
    {
      msg_warning("flip-parser(): flip_text(no) reverse_text(no) does nothing with the message at all");
    }

  return log_parser_init_method(s);
}


LogParser *
flip_parser_new(GlobalConfig *cfg)
{
  FlipParser *self = g_new0(FlipParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = flip_parser_init;
  self->super.super.clone = flip_parser_clone;
  self->super.process = flip_parser_process;

  self->reverse = TRUE;
  self->flip = TRUE;

  return &self->super;
}
