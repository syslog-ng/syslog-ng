/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <bazsi@balabit.hu>
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
#include "logproto/logproto-multiline-server.h"
#include "logproto/logproto-regexp-multiline-server.h"
#include "logproto/logproto-indented-multiline-server.h"

/*
 * This is basically a factory that takes multi-line related options and
 * constructs the appropriate LogProtoServer instance.
 */

LogProtoServer *
log_proto_multiline_server_new(LogTransport *transport,
                               const LogProtoMultiLineServerOptions *options)
{
  switch (options->mode)
    {
    case MLM_INDENTED:
      return log_proto_indented_multiline_server_new(transport, &options->super);
    case MLM_PREFIX_GARBAGE:
      return log_proto_prefix_garbage_multiline_server_new(transport, &options->super,
                                                           options->prefix,
                                                           options->garbage);
    case MLM_PREFIX_SUFFIX:
      return log_proto_prefix_suffix_multiline_server_new(transport, &options->super,
                                                          options->prefix,
                                                          options->garbage);
    case MLM_NONE:
      return log_proto_text_server_new(transport, &options->super);

    default:
      g_assert_not_reached();
      break;
    }
  g_assert_not_reached();
}

gboolean
log_proto_multi_line_server_options_set_mode(LogProtoMultiLineServerOptions *options, const gchar *mode)
{
  if (strcasecmp(mode, "indented") == 0)
    options->mode = MLM_INDENTED;
  else if (strcasecmp(mode, "regexp") == 0)
    options->mode = MLM_PREFIX_GARBAGE;
  else if (strcasecmp(mode, "prefix-garbage") == 0)
    options->mode = MLM_PREFIX_GARBAGE;
  else if (strcasecmp(mode, "prefix-suffix") == 0)
    options->mode = MLM_PREFIX_SUFFIX;
  else if (strcasecmp(mode, "none") == 0)
    options->mode = MLM_NONE;
  else
    return FALSE;
  return TRUE;
}

gboolean
log_proto_multi_line_server_options_set_prefix(LogProtoMultiLineServerOptions *options, const gchar *prefix_regexp,
                                               GError **error)
{
  options->prefix = multi_line_regexp_compile(prefix_regexp, error);
  return options->prefix != NULL;
}

gboolean
log_proto_multi_line_server_options_set_garbage(LogProtoMultiLineServerOptions *options, const gchar *garbage_regexp,
                                                GError **error)
{
  options->garbage = multi_line_regexp_compile(garbage_regexp, error);
  return options->garbage != NULL;
}

void
log_proto_multi_line_server_options_defaults(LogProtoMultiLineServerOptions *options)
{
  options->mode = MLM_NONE;
}

void
log_proto_multi_line_server_options_init(LogProtoMultiLineServerOptions *options)
{
}

void
log_proto_multi_line_server_options_destroy(LogProtoMultiLineServerOptions *options)
{
  multi_line_regexp_free(options->prefix);
  multi_line_regexp_free(options->garbage);
}
