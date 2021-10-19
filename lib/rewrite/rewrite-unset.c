/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 1998-2016 Balázs Scheidler
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

#include "rewrite-unset.h"

/* LogRewriteUnset
 *
 * This class implements the "unset" expression in a rewrite rule.
 */
typedef struct _LogRewriteUnset LogRewriteUnset;

struct _LogRewriteUnset
{
  LogRewrite super;
};

static void
log_rewrite_unset_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteUnset *self = (LogRewriteUnset *) s;

  log_msg_make_writable(pmsg, path_options);
  log_msg_unset_value(*pmsg, self->super.value_handle);
}

static LogPipe *
log_rewrite_unset_clone(LogPipe *s)
{
  LogRewriteUnset *self = (LogRewriteUnset *) s;
  LogRewriteUnset *cloned;

  cloned = (LogRewriteUnset *) log_rewrite_unset_new(s->cfg);
  cloned->super.value_handle = self->super.value_handle;

  if (self->super.condition)
    cloned->super.condition = filter_expr_clone(self->super.condition);

  return &cloned->super.super;
}

LogRewrite *
log_rewrite_unset_new(GlobalConfig *cfg)
{
  LogRewriteUnset *self = g_new0(LogRewriteUnset, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.clone = log_rewrite_unset_clone;
  self->super.process = log_rewrite_unset_process;
  return &self->super;
}
