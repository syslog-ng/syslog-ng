/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balazs Scheidler <bazsi@balabit.hu>
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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
#include "regexp-multi-line.h"

MultiLinePattern *
multi_line_pattern_compile(const gchar *regexp, GError **error)
{
  MultiLinePattern *self = g_new0(MultiLinePattern, 1);
  gint optflags = 0;
  gint rc;
  const gchar *errptr;
  gint erroffset;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  self->ref_cnt = 1;

  /* compile the regexp */
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

MultiLinePattern *
multi_line_pattern_ref(MultiLinePattern *self)
{
  if (self)
    self->ref_cnt++;
  return self;
}

void
multi_line_pattern_unref(MultiLinePattern *self)
{
  if (self && (--self->ref_cnt == 0))
    {
      if (self->pattern)
        pcre_free(self->pattern);
      if (self->extra)
        pcre_free_study(self->extra);
      g_free(self);
    }
}

static int
_find_regexp(MultiLinePattern *re, const guchar *str, gsize len, gint *matches, gint matches_num)
{
  gint rc;

  if (!re)
    return -1;

  rc = pcre_exec(re->pattern, re->extra, (const gchar *) str, len, 0, 0, matches, matches_num * 3);
  return rc;
}

static gboolean
_regexp_matches(MultiLinePattern *re, const guchar *str, gsize len)
{
  gint match[3];
  if (_find_regexp(re, str, len, match, 1) < 0)
    return FALSE;
  return match[0] >= 0;
}

static gint
_prefix_garbage_get_offset_of_garbage(RegexpMultiLine *self, const guchar *line, gsize line_len)
{
  gint match[3];
  if (_find_regexp(self->garbage, line, line_len, match, 1) < 0)
    return -1;
  return match[0];
}

static gint
_prefix_suffix_get_offset_of_garbage(RegexpMultiLine *self, const guchar *line, gsize line_len)
{
  gint match[3];
  if (_find_regexp(self->garbage, line, line_len, match, 1) < 0)
    return -1;
  return match[1];
}

static gint
_get_offset_of_garbage(RegexpMultiLine *self, const guchar *line, gsize line_len)
{
  if (self->mode == RML_PREFIX_GARBAGE)
    return _prefix_garbage_get_offset_of_garbage(self, line, line_len);
  else
    return _prefix_suffix_get_offset_of_garbage(self, line, line_len);
}

static gint
_accumulate_initial_line(RegexpMultiLine *self,
                         const guchar *line,
                         gsize line_len)
{
  gint offset_of_garbage = _get_offset_of_garbage(self, line, line_len);
  if (offset_of_garbage >= 0)
    return MLL_CONSUME_PARTIALLY(line_len - offset_of_garbage) | MLL_EXTRACTED;
  else
    return MLL_CONSUME_SEGMENT | MLL_WAITING;

}

static gint
_accumulate_continuation_line(RegexpMultiLine *self,
                              const guchar *line,
                              gsize line_len)
{
  gint offset_of_garbage = _get_offset_of_garbage(self, line, line_len);
  if (offset_of_garbage >= 0)
    return MLL_CONSUME_PARTIALLY(line_len - offset_of_garbage) | MLL_EXTRACTED;
  else if (_regexp_matches(self->prefix, line, line_len))
    return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
  else
    return MLL_CONSUME_SEGMENT | MLL_WAITING;
}

static gint
regexp_multi_line_accumulate_line(MultiLineLogic *s,
                                  const guchar *msg,
                                  gsize msg_len,
                                  const guchar *segment,
                                  gsize segment_len)
{
  RegexpMultiLine *self = (RegexpMultiLine *) s;
  if (msg_len == 0)
    {
      return _accumulate_initial_line(self, segment, segment_len);
    }
  else
    {
      return _accumulate_continuation_line(self, segment, segment_len);
    }
}

static void
regexp_multi_line_free(MultiLineLogic *s)
{
  RegexpMultiLine *self = (RegexpMultiLine *) s;

  multi_line_pattern_unref(self->prefix);
  multi_line_pattern_unref(self->garbage);
  multi_line_logic_free_method(s);
}

MultiLineLogic *
regexp_multi_line_new(gint mode, MultiLinePattern *prefix, MultiLinePattern *garbage_or_suffix)
{
  RegexpMultiLine *self = g_new0(RegexpMultiLine, 1);

  multi_line_logic_init_instance(&self->super);
  self->super.accumulate_line = regexp_multi_line_accumulate_line;
  self->super.free_fn = regexp_multi_line_free;
  self->mode = mode;
  self->prefix = multi_line_pattern_ref(prefix);
  self->garbage = multi_line_pattern_ref(garbage_or_suffix);
  return &self->super;
}
