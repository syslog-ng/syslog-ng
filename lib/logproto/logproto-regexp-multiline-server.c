/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Balazs Scheidler <bazsi@balabit.hu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "logproto-regexp-multiline-server.h"
#include "messages.h"
#include "misc.h"

static gint
_find_regexp(regex_t *re, const guchar *str, gsize len)
{
  gint rc;
  gchar *buf;
  regmatch_t matches[1];

  if (!re)
    return -1;

  APPEND_ZERO(buf, str, len);
  rc = regexec(re, buf, sizeof(matches) / sizeof(matches[0]), matches, 0);
  if (rc == 0)
    return matches[0].rm_so;
  return -1;

}

static gboolean
_regexp_matches(regex_t *re, const guchar *str, gsize len)
{
  return _find_regexp(re, str, len) >= 0;
}

static gint
_accumulate_initial_line(LogProtoREMultiLineServer *self,
                                                   const guchar *line,
                                                   gsize line_len,
                                                   gssize consumed_len)
{
  gint offset_of_garbage;

  offset_of_garbage = _find_regexp(self->garbage, line, line_len);
  if (offset_of_garbage >= 0)
    return LPT_CONSUME_PARTIALLY(line_len - offset_of_garbage) | LPT_EXTRACTED;
  else
    return LPT_CONSUME_LINE | LPT_WAITING;

}

static gint
_accumulate_continuation_line(LogProtoREMultiLineServer *self,
                                                        const guchar *line,
                                                        gsize line_len,
                                                        gssize consumed_len)
{
  gint offset_of_garbage;

  offset_of_garbage = _find_regexp(self->garbage, line, line_len);
  if (offset_of_garbage >= 0)
    return LPT_CONSUME_PARTIALLY(line_len - offset_of_garbage) | LPT_EXTRACTED;
  else if (_regexp_matches(self->prefix, line, line_len))
    return LPT_REWIND_LINE | LPT_EXTRACTED;
  else
    return LPT_CONSUME_LINE | LPT_WAITING;
}

static gint
log_proto_regexp_multiline_accumulate_line(LogProtoTextServer *s,
                                           const guchar *msg,
                                           gsize msg_len,
                                           gssize consumed_len)
{
  LogProtoREMultiLineServer *self = (LogProtoREMultiLineServer *) s;
  gboolean initial_line;

  /* NOTES:
   *
   *    msg
   *      points to the beginning of the line _repeatedly_, e.g. as
   *      long as we return the we couldn't extract a message.
   *
   *    msg_len
   *      This is getting longer and longer as lines get accumulated
   *      in the message.
   *
   *    consumed_len
   *      Is the length of the message starting with "msg" that was already
   *      consumed by this function.  In practice this points to the EOL
   *      character of the last consumed line.
   *
   */

  initial_line = (consumed_len < 0) || (msg_len <= consumed_len + 1);
  if (initial_line)
    {
      return _accumulate_initial_line(self, msg, msg_len, consumed_len);
    }
  else
    {
      const guchar *start_of_the_current_line = &msg[consumed_len + 1];
      gint length_of_the_current_line = msg_len - (start_of_the_current_line - msg);

      return _accumulate_continuation_line(self, start_of_the_current_line, length_of_the_current_line, consumed_len);
    }
}

void
log_proto_regexp_multiline_server_init(LogProtoREMultiLineServer *self,
                                       LogTransport *transport,
                                       const LogProtoServerOptions *options,
                                       regex_t *prefix,
                                       regex_t *garbage)
{
  log_proto_text_server_init(&self->super, transport, options);
  self->super.accumulate_line = log_proto_regexp_multiline_accumulate_line;
  self->prefix = prefix;
  self->garbage = garbage;
}

LogProtoServer *
log_proto_regexp_multiline_server_new(LogTransport *transport,
                                      const LogProtoServerOptions *options,
                                      regex_t *prefix,
                                      regex_t *garbage)
{
  LogProtoREMultiLineServer *self = g_new0(LogProtoREMultiLineServer, 1);

  log_proto_regexp_multiline_server_init(self, transport, options, prefix, garbage);
  return &self->super.super.super;
}
