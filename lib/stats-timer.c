/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "stats-timer.h"
#include "stats.h"

#include <iv.h>

static time_t stats_lifetime;

static void
stats_timer_rearm(struct iv_timer *timer)
{
  gint freq = GPOINTER_TO_INT(timer->cookie);
  if (freq > 0)
    {
      /* arm the timer */
      iv_validate_now();
      timer->expires = iv_now;
      timespec_add_msec(&timer->expires, freq * 1000);
      iv_timer_register(timer);
    }
}

static void
stats_timer_init(struct iv_timer *timer, void (*handler)(void *), gint freq)
{
  IV_TIMER_INIT(timer);
  timer->handler = handler;
  timer->cookie = GINT_TO_POINTER(freq);
}

static void
stats_timer_kill(struct iv_timer *timer)
{
  if (!timer->handler)
    return;
  if (iv_timer_registered(timer))
    iv_timer_unregister(timer);
}

static struct iv_timer stats_log_timer;
static struct iv_timer stats_prune_timer;


static void
stats_log_timer_elapsed(gpointer st)
{
  stats_generate_log();
  stats_timer_rearm(&stats_log_timer);
}

static void
stats_prune_timer_elapsed(gpointer st)
{
  stats_prune_old_counters(stats_lifetime);
  stats_timer_rearm(&stats_prune_timer);
}

void
stats_timer_reinit(gint stats_freq, gint _stats_lifetime)
{
  stats_timer_kill(&stats_log_timer);
  stats_timer_init(&stats_log_timer, stats_log_timer_elapsed, stats_freq);
  stats_timer_rearm(&stats_log_timer);

  stats_lifetime = _stats_lifetime;
  stats_timer_kill(&stats_prune_timer);
  stats_timer_init(&stats_prune_timer, stats_prune_timer_elapsed, stats_lifetime == 1 ? 1 : stats_lifetime / 2);
  stats_timer_rearm(&stats_prune_timer);
}
