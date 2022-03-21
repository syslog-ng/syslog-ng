/*
 * Copyright (c) 2022 Balázs Scheidler <bazsi77@gmail.com>
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

#include "rewrite-unset-matches.h"

/* LogRewriteUnsetMatches
 *
 * This class implements the "unset-matches" expression in a rewrite rule.
 */
typedef struct _LogRewriteUnsetMatches LogRewriteUnsetMatches;

struct _LogRewriteUnsetMatches
{
  LogRewrite super;
};

static void
log_rewrite_unset_matches_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  log_msg_make_writable(pmsg, path_options);
  (*pmsg)->num_matches = 0;
}

static LogPipe *
log_rewrite_unset_matches_clone(LogPipe *s)
{
  LogRewriteUnsetMatches *self = (LogRewriteUnsetMatches *) s;
  LogRewriteUnsetMatches *cloned;

  cloned = (LogRewriteUnsetMatches *) log_rewrite_unset_matches_new(s->cfg);
  cloned->super.value_handle = self->super.value_handle;

  if (self->super.condition)
    cloned->super.condition = filter_expr_clone(self->super.condition);

  return &cloned->super.super;
}

LogRewrite *
log_rewrite_unset_matches_new(GlobalConfig *cfg)
{
  LogRewriteUnsetMatches *self = g_new0(LogRewriteUnsetMatches, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.clone = log_rewrite_unset_matches_clone;
  self->super.process = log_rewrite_unset_matches_process;
  return &self->super;
}
