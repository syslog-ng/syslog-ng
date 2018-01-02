/*
 * Copyright (c) 2013 Balabit
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
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#include "logproto-regexp-multiline-server.h"
#include "messages.h"
#include "compat/pcre.h"

#include <string.h>

struct _MultiLineRegexp
{
  pcre *pattern;
  pcre_extra *extra;
};

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
      g_set_error(error, 0, 0, "Error while compiling multi-line regexp as a PCRE expression, error=%s, error_at=%d", errptr,
                  erroffset);
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
        pcre_free_study(self->extra);
      g_free(self);
    }
}

struct _LogProtoREMultiLineServer
{
  LogProtoTextServer super;
  /* these are borrowed */
  MultiLineRegexp *prefix;
  MultiLineRegexp *garbage;
  gint (*get_offset_of_garbage)(LogProtoREMultiLineServer *self, const guchar *line, gsize line_len);
};

static int
_find_regexp(MultiLineRegexp *re, const guchar *str, gsize len, gint *matches, gint matches_num)
{
  gint rc;

  if (!re)
    return -1;

  rc = pcre_exec(re->pattern, re->extra, (const gchar *) str, len, 0, 0, matches, matches_num * 3);
  return rc;
}

static gboolean
_regexp_matches(MultiLineRegexp *re, const guchar *str, gsize len)
{
  gint match[3];
  if (_find_regexp(re, str, len, match, 1) < 0)
    return FALSE;
  return match[0] >= 0;
}

gint
log_proto_prefix_garbage_multiline_get_offset_of_garbage(LogProtoREMultiLineServer *self, const guchar *line,
                                                         gsize line_len)
{
  gint match[3];
  if (_find_regexp(self->garbage, line, line_len, match, 1) < 0)
    return -1;
  return match[0];
};

gint
log_proto_prefix_suffix_multiline_get_offset_of_garbage(LogProtoREMultiLineServer *self, const guchar *line,
                                                        gsize line_len)
{
  gint match[3];
  if (_find_regexp(self->garbage, line, line_len, match, 1) < 0)
    return -1;
  return match[1];
};


static gint
_accumulate_initial_line(LogProtoREMultiLineServer *self,
                         const guchar *line,
                         gsize line_len,
                         gssize consumed_len)
{
  gint offset_of_garbage = self->get_offset_of_garbage(self, line, line_len);
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
  gint offset_of_garbage = self->get_offset_of_garbage(self, line, line_len);
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

static gboolean
log_proto_regexp_multi_line_server_validate_options(LogProtoServer *s)
{
  LogProtoREMultiLineServer *self = (LogProtoREMultiLineServer *) s;

  if (!self->prefix &&
      !self->garbage)
    {
      msg_error("To follow files in multi-line-mode() 'regexp', 'prefix-garbage', 'prefix-suffix', "
                "please also specify regexps using the multi-line-prefix/garbage options");
      return FALSE;
    }
  return log_proto_text_server_validate_options_method(s);
}

void
log_proto_regexp_multiline_server_init(LogProtoREMultiLineServer *self,
                                       LogTransport *transport,
                                       const LogProtoServerOptions *options,
                                       MultiLineRegexp *prefix,
                                       MultiLineRegexp *garbage_or_suffix)
{
  log_proto_text_server_init(&self->super, transport, options);
  self->super.super.super.validate_options = log_proto_regexp_multi_line_server_validate_options;
  self->super.accumulate_line = log_proto_regexp_multiline_accumulate_line;
  self->prefix = prefix;
  self->garbage = garbage_or_suffix;
}

LogProtoServer *
log_proto_prefix_garbage_multiline_server_new(LogTransport *transport,
                                              const LogProtoServerOptions *options,
                                              MultiLineRegexp *prefix,
                                              MultiLineRegexp *garbage)
{
  LogProtoREMultiLineServer *self = g_new0(LogProtoREMultiLineServer, 1);

  log_proto_regexp_multiline_server_init(self, transport, options, prefix, garbage);
  self->get_offset_of_garbage = log_proto_prefix_garbage_multiline_get_offset_of_garbage;
  return &self->super.super.super;
}

LogProtoServer *
log_proto_prefix_suffix_multiline_server_new(LogTransport *transport,
                                             const LogProtoServerOptions *options,
                                             MultiLineRegexp *prefix,
                                             MultiLineRegexp *suffix)
{
  LogProtoREMultiLineServer *self = g_new0(LogProtoREMultiLineServer, 1);

  log_proto_regexp_multiline_server_init(self, transport, options, prefix, suffix);
  self->get_offset_of_garbage = log_proto_prefix_suffix_multiline_get_offset_of_garbage;
  return &self->super.super.super;
}
