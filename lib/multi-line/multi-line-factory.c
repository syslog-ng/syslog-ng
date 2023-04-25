/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <bazsi@balabit.hu>
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
 *
 */

#include "multi-line/multi-line-factory.h"
#include "multi-line/regexp-multi-line.h"
#include "multi-line/indented-multi-line.h"
#include "multi-line/smart-multi-line.h"
#include "messages.h"

#include <string.h>
#include <strings.h>

MultiLineLogic *
multi_line_factory_construct(const MultiLineOptions *options)
{
  switch (options->mode)
    {
    case MLM_INDENTED:
      return indented_multi_line_new();
    case MLM_REGEXP_PREFIX_GARBAGE:
      return regexp_multi_line_new(RML_PREFIX_GARBAGE, options->regexp.prefix, options->regexp.garbage);
    case MLM_REGEXP_PREFIX_SUFFIX:
      return regexp_multi_line_new(RML_PREFIX_SUFFIX, options->regexp.prefix, options->regexp.garbage);
    case MLM_SMART:
      return smart_multi_line_new();
    case MLM_NONE:
      return NULL;

    default:
      g_assert_not_reached();
      break;
    }
  g_assert_not_reached();
}

gboolean
multi_line_options_set_mode(MultiLineOptions *options, const gchar *mode)
{
  if (strcasecmp(mode, "indented") == 0)
    options->mode = MLM_INDENTED;
  else if (strcasecmp(mode, "regexp") == 0)
    options->mode = MLM_REGEXP_PREFIX_GARBAGE;
  else if (strcasecmp(mode, "prefix-garbage") == 0)
    options->mode = MLM_REGEXP_PREFIX_GARBAGE;
  else if (strcasecmp(mode, "prefix-suffix") == 0)
    options->mode = MLM_REGEXP_PREFIX_SUFFIX;
  else if (strcasecmp(mode, "smart") == 0)
    options->mode = MLM_SMART;
  else if (strcasecmp(mode, "none") == 0)
    options->mode = MLM_NONE;
  else
    return FALSE;
  return TRUE;
}

gboolean
multi_line_options_set_prefix(MultiLineOptions *options, const gchar *prefix_regexp,
                              GError **error)
{
  multi_line_pattern_unref(options->regexp.prefix);
  options->regexp.prefix = multi_line_pattern_compile(prefix_regexp, error);
  return options->regexp.prefix != NULL;
}

gboolean
multi_line_options_set_garbage(MultiLineOptions *options, const gchar *garbage_regexp,
                               GError **error)
{
  multi_line_pattern_unref(options->regexp.garbage);
  options->regexp.garbage = multi_line_pattern_compile(garbage_regexp, error);
  return options->regexp.garbage != NULL;
}

gboolean
multi_line_options_validate(MultiLineOptions *options)
{
  gboolean is_garbage_mode = options->mode == MLM_REGEXP_PREFIX_GARBAGE;
  gboolean is_suffix_mode = options->mode == MLM_REGEXP_PREFIX_SUFFIX;

  if ((!is_garbage_mode && !is_suffix_mode) && (options->regexp.prefix || options->regexp.garbage))
    {
      msg_error("multi-line-prefix() and/or multi-line-garbage() specified but multi-line-mode() is not regexp based "
                "(prefix-garbage or prefix-suffix), please set multi-line-mode() properly");
      return FALSE;
    }
  return TRUE;
}

void
multi_line_options_defaults(MultiLineOptions *options)
{
  memset(options, 0, sizeof(*options));
  options->mode = MLM_NONE;
}

void
multi_line_options_copy(MultiLineOptions *dest, MultiLineOptions *source)
{
  dest->mode = source->mode;
  if (dest->mode == MLM_REGEXP_PREFIX_GARBAGE || dest->mode == MLM_REGEXP_PREFIX_SUFFIX)
    {
      dest->regexp.prefix = multi_line_pattern_ref(source->regexp.prefix);
      dest->regexp.garbage = multi_line_pattern_ref(source->regexp.garbage);
    }
}

gboolean
multi_line_options_init(MultiLineOptions *options)
{
  if (!multi_line_options_validate(options))
    return FALSE;
  return TRUE;
}

void
multi_line_options_destroy(MultiLineOptions *options)
{
  multi_line_pattern_unref(options->regexp.prefix);
  multi_line_pattern_unref(options->regexp.garbage);
}

void
multi_line_global_init(void)
{
  smart_multi_line_global_init();
}

void
multi_line_global_deinit(void)
{
  smart_multi_line_global_deinit();
}
