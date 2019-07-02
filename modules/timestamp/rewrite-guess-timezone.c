/*
 * Copyright (c) 2019 Balazs Scheidler <bazsi77@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "rewrite-guess-timezone.h"
#include "messages.h"

typedef struct _RewriteGuessTimeZone RewriteGuessTimeZone;

struct _RewriteGuessTimeZone
{
  LogRewrite super;
  gint stamp;
};

void
rewrite_guess_time_zone_set_time_stamp(LogRewrite *s, gint stamp)
{
  RewriteGuessTimeZone *self = (RewriteGuessTimeZone *) s;
  self->stamp = stamp;
}

static void
_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  RewriteGuessTimeZone *self = (RewriteGuessTimeZone *) s;
  LogMessage *msg;

  msg = log_msg_make_writable(pmsg, path_options);

  glong implied_gmtoff = msg->timestamps[self->stamp].ut_gmtoff;
  unix_time_fix_timezone_assuming_the_time_matches_real_time(&msg->timestamps[self->stamp]);
  msg_trace("guess-timezone(): adjusting message timezone assuming it was real time",
            evt_tag_long("implied_gmtoff", implied_gmtoff),
            evt_tag_long("new_gmtoff", msg->timestamps[self->stamp].ut_gmtoff));
}

static LogPipe *
_clone(LogPipe *s)
{
  RewriteGuessTimeZone *self = (RewriteGuessTimeZone *) s;
  LogRewrite *cloned;

  cloned = rewrite_guess_time_zone_new(s->cfg);

  rewrite_guess_time_zone_set_time_stamp(cloned, self->stamp);
  if (self->super.condition)
    cloned->condition = filter_expr_ref(self->super.condition);

  return &cloned->super;
}

LogRewrite *
rewrite_guess_time_zone_new(GlobalConfig *cfg)
{
  RewriteGuessTimeZone *self = g_new0(RewriteGuessTimeZone, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.clone = _clone;
  self->super.process = _process;
  return &self->super;
}
