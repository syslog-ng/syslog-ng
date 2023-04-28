/*
 * Copyright (c) 2021 Balabit
 * Copyright (c) 2021 Kokan <kokaipeter@gmail.com>
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

#include "rewrite-rename.h"

typedef struct _LogRewriteRename LogRewriteRename;

struct _LogRewriteRename
{
  LogRewrite super;
  NVHandle source_handle;
  NVHandle destination_handle;
};

static void
log_rewrite_rename_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteRename *self = (LogRewriteRename *) s;

  if (self->source_handle == self->destination_handle)
    return;

  log_msg_make_writable(pmsg, path_options);

  log_msg_rename_value(*pmsg, self->source_handle, self->destination_handle);
}

static LogPipe *
log_rewrite_rename_clone(LogPipe *s)
{
  LogRewriteRename *self = (LogRewriteRename *) s;
  LogRewriteRename *cloned;

  cloned = (LogRewriteRename *) log_rewrite_rename_new(s->cfg, self->source_handle, self->destination_handle);
  log_rewrite_clone_method(&cloned->super, &self->super);

  return &cloned->super.super;
}

static gboolean
log_rewrite_rename_init(LogPipe *s)
{
  LogRewriteRename *self = (LogRewriteRename *) s;
  if (!self->source_handle || !self->destination_handle)
    {
      msg_error("rename(from to): requires two non-empty arguments", log_pipe_location_tag(s));
      return FALSE;
    }

  return TRUE;
}

LogRewrite *
log_rewrite_rename_new(GlobalConfig *cfg, NVHandle source, NVHandle destination)
{
  LogRewriteRename *self = g_new0(LogRewriteRename, 1);

  self->source_handle = source;
  self->destination_handle = destination;

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.init = log_rewrite_rename_init;
  self->super.super.clone = log_rewrite_rename_clone;
  self->super.process = log_rewrite_rename_process;
  return &self->super;
}
