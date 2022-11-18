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
#include "multi-line/regexp-multi-line.h"
#include "messages.h"

#include <string.h>

struct _LogProtoREMultiLineServer
{
  LogProtoTextServer super;
};

static gboolean
log_proto_regexp_multi_line_server_validate_options(LogProtoServer *s)
{
#if 0
  LogProtoREMultiLineServer *self = (LogProtoREMultiLineServer *) s;

  if (!self->prefix &&
      !self->garbage)
    {
      msg_error("To follow files in multi-line-mode() 'regexp', 'prefix-garbage', 'prefix-suffix', "
                "please also specify regexps using the multi-line-prefix/garbage options");
      return FALSE;
    }
#endif
  return log_proto_text_server_validate_options_method(s);
}

void
log_proto_regexp_multiline_server_init(LogProtoREMultiLineServer *self,
                                       LogTransport *transport,
                                       const LogProtoServerOptions *options,
                                       gint mode,
                                       MultiLinePattern *prefix,
                                       MultiLinePattern *garbage_or_suffix)
{
  log_proto_text_server_init(&self->super, transport, options);
  self->super.multi_line = regexp_multi_line_new(mode, prefix, garbage_or_suffix);
  self->super.super.super.validate_options = log_proto_regexp_multi_line_server_validate_options;
}

LogProtoServer *
log_proto_prefix_garbage_multiline_server_new(LogTransport *transport,
                                              const LogProtoServerOptions *options,
                                              MultiLinePattern *prefix,
                                              MultiLinePattern *garbage)
{
  LogProtoREMultiLineServer *self = g_new0(LogProtoREMultiLineServer, 1);

  log_proto_regexp_multiline_server_init(self, transport, options, RML_PREFIX_GARBAGE, prefix, garbage);
  return &self->super.super.super;
}

LogProtoServer *
log_proto_prefix_suffix_multiline_server_new(LogTransport *transport,
                                             const LogProtoServerOptions *options,
                                             MultiLinePattern *prefix,
                                             MultiLinePattern *suffix)
{
  LogProtoREMultiLineServer *self = g_new0(LogProtoREMultiLineServer, 1);

  log_proto_regexp_multiline_server_init(self, transport, options, RML_PREFIX_SUFFIX, prefix, suffix);
  return &self->super.super.super;
}
