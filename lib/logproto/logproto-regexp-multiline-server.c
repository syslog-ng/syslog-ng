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

#include <string.h>
#include <pcre.h>

typedef struct _MultiLineRegexp
{
  pcre *pattern;
  pcre_extra *extra;
} MultiLineRegexp;

MultiLineRegexp *
multi_line_regexp_compile(const gchar *regexp, GError **error)
{
  MultiLineRegexp *self = g_new0(MultiLineRegexp, 1);
  gint optflags = 0;
  gint rc;
  const gchar *errptr;
  gint erroffset;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  /* complile the regexp */
  self->pattern = pcre_compile2(regexp, 0, &rc, &errptr, &erroffset, NULL);
  if (!self->pattern)
    {
      g_set_error(error, 0, 0, "Error while compiling multi-line regexp as a PCRE expression, error=%s, error_at=%d", errptr, erroffset);
      goto error;
    }

#ifdef PCRE_STUDY_JIT_COMPILE
  optflags = PCRE_STUDY_JIT_COMPILE;
#endif

  /* optimize regexp */
  self->extra = pcre_study(self->pattern, optflags, &errptr);
  if (errptr != NULL)
    {
      g_set_error(error, 0, 0, "Error while studying multi-line regexp, error=%s", errptr);
      goto error;
    }

  return self;
 error:
  if (self->pattern)
    pcre_free(self->pattern);
  g_free(self);
  return NULL;
}

void
multi_line_regexp_free(MultiLineRegexp *self)
{
  if (self)
    {
      if (self->pattern)
        pcre_free(self->pattern);
      if (self->extra)
        pcre_free(self->extra);
      g_free(self);
    }
}

struct _LogProtoREMultiLineServer
{
  LogProtoTextServer super;
  /* these are borrowed */
  MultiLineRegexp *prefix;
  MultiLineRegexp *garbage;
};

static gint
_find_regexp(MultiLineRegexp *re, const gchar *str, gsize len)
{
  gint rc;
  gint matches[30];

  if (!re)
    return -1;

  rc = pcre_exec(re->pattern, re->extra, str, len, 0, 0, matches, 10);
  if (rc >= 0)
    return matches[0];
  return -1;

}

static gboolean
_regexp_matches(MultiLineRegexp *re, const gchar *str, gsize len)
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

  offset_of_garbage = _find_regexp(self->garbage, (const gchar *) line, line_len);
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

  offset_of_garbage = _find_regexp(self->garbage, (const gchar *) line, line_len);
  if (offset_of_garbage >= 0)
    return LPT_CONSUME_PARTIALLY(line_len - offset_of_garbage) | LPT_EXTRACTED;
  else if (_regexp_matches(self->prefix, (const gchar *) line, line_len))
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
                                       MultiLineRegexp *prefix,
                                       MultiLineRegexp *garbage)
{
  log_proto_text_server_init(&self->super, transport, options);
  self->super.accumulate_line = log_proto_regexp_multiline_accumulate_line;
  self->prefix = prefix;
  self->garbage = garbage;
}

LogProtoServer *
log_proto_regexp_multiline_server_new(LogTransport *transport,
                                      const LogProtoServerOptions *options,
                                      MultiLineRegexp *prefix,
                                      MultiLineRegexp *garbage)
{
  LogProtoREMultiLineServer *self = g_new0(LogProtoREMultiLineServer, 1);

  log_proto_regexp_multiline_server_init(self, transport, options, prefix, garbage);
  return &self->super.super.super;
}
