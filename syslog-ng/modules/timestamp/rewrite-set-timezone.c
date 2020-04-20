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
#include "rewrite-set-timezone.h"
#include "scratch-buffers.h"
#include "timeutils/cache.h"

typedef struct _RewriteSetTimeZone RewriteSetTimeZone;

struct _RewriteSetTimeZone
{
  LogRewrite super;
  LogTemplate *zone_template;
  gint stamp;
};

void
rewrite_set_time_zone_set_zone_template_ref(LogRewrite *s, LogTemplate *zone_template)
{
  RewriteSetTimeZone *self = (RewriteSetTimeZone *) s;

  log_template_unref(self->zone_template);
  self->zone_template = zone_template;
}

void
rewrite_set_time_zone_set_time_stamp(LogRewrite *s, gint stamp)
{
  RewriteSetTimeZone *self = (RewriteSetTimeZone *) s;
  self->stamp = stamp;
}

static void
_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  RewriteSetTimeZone *self = (RewriteSetTimeZone *) s;
  GString *result = scratch_buffers_alloc();
  LogMessage *msg = *pmsg;

  log_template_format(self->zone_template, *pmsg, NULL, LTZ_LOCAL, 0, NULL, result);

  UnixTime stamp = msg->timestamps[self->stamp];
  glong orig_gmtoff = stamp.ut_gmtoff;
  TimeZoneInfo *tzinfo = cached_get_time_zone_info(result->str);

  unix_time_set_timezone_with_tzinfo(&stamp, tzinfo);
  if (stamp.ut_gmtoff != msg->timestamps[self->stamp].ut_gmtoff)
    {
      msg = log_msg_make_writable(pmsg, path_options);

      msg->timestamps[self->stamp] = stamp;
    }
  msg_trace("set-timezone(): adjusting message timezone to a different value",
            evt_tag_str("new_timezone", result->str),
            evt_tag_long("orig_gmtoff", orig_gmtoff),
            evt_tag_long("new_gmtoff", stamp.ut_gmtoff));
}

static LogPipe *
_clone(LogPipe *s)
{
  RewriteSetTimeZone *self = (RewriteSetTimeZone *) s;
  LogRewrite *cloned;

  cloned = rewrite_set_time_zone_new(s->cfg);

  rewrite_set_time_zone_set_zone_template_ref(cloned, log_template_ref(self->zone_template));
  rewrite_set_time_zone_set_time_stamp(cloned, self->stamp);
  if (self->super.condition)
    cloned->condition = filter_expr_ref(self->super.condition);

  return &cloned->super;
}

static void
_free(LogPipe *s)
{
  RewriteSetTimeZone *self = (RewriteSetTimeZone *) s;

  log_template_unref(self->zone_template);
  log_rewrite_free_method(s);
}

LogRewrite *
rewrite_set_time_zone_new(GlobalConfig *cfg)
{
  RewriteSetTimeZone *self = g_new0(RewriteSetTimeZone, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = _free;
  self->super.super.clone = _clone;
  self->super.process = _process;
  self->stamp = LM_TS_STAMP;
  return &self->super;
}
