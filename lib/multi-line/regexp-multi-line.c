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


static gint
_prefix_garbage_get_offset_of_garbage(RegexpMultiLine *self, const guchar *line, gsize line_len)
{
  gint start, end;

  if (!multi_line_pattern_find(self->garbage, line, line_len, &start, &end))
    return -1;
  return start;
}

static gint
_prefix_suffix_get_offset_of_garbage(RegexpMultiLine *self, const guchar *line, gsize line_len)
{
  gint start, end;

  if (!multi_line_pattern_find(self->garbage, line, line_len, &start, &end))
    return -1;
  return end;
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
  else if (multi_line_pattern_match(self->prefix, line, line_len))
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
