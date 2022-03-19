/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
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
#include "correlation.h"
#include "correlation-key.h"
#include "correlation-context.h"
#include "timeutils/cache.h"
#include "timeutils/misc.h"

void
correlation_state_set_time(CorrelationState *self, guint64 sec, gpointer caller_context)
{
  GTimeVal now;

  /* clamp the current time between the timestamp of the current message
   * (low limit) and the current system time (high limit).  This ensures
   * that incorrect clocks do not skew the current time know by the
   * correlation engine too much. */

  cached_g_current_time(&now);
  self->last_tick = now;

  if (sec < now.tv_sec)
    now.tv_sec = sec;

  timer_wheel_set_time(self->timer_wheel, now.tv_sec, caller_context);
}

gboolean
correlation_state_timer_tick(CorrelationState *self, gpointer caller_context)
{
  GTimeVal now;
  glong diff;
  gboolean updated = FALSE;

  g_mutex_lock(&self->lock);
  cached_g_current_time(&now);
  diff = g_time_val_diff(&now, &self->last_tick);

  if (diff > 1e6)
    {
      glong diff_sec = (glong)(diff / 1e6);

      timer_wheel_set_time(self->timer_wheel, timer_wheel_get_time(self->timer_wheel) + diff_sec, caller_context);
      /* update last_tick, take the fraction of the seconds not calculated into this update into account */

      self->last_tick = now;
      g_time_val_add(&self->last_tick, - (glong)(diff - diff_sec * 1e6));
      updated = TRUE;
    }
  else if (diff < 0)
    {
      /* time moving backwards, this can only happen if the computer's time
       * is changed.  We don't update patterndb's idea of the time now, wait
       * another tick instead to update that instead.
       */
      self->last_tick = now;
    }
  g_mutex_unlock(&self->lock);
  return updated;
}

void
correlation_state_init_instance(CorrelationState *self)
{
  g_mutex_init(&self->lock);
  self->state = g_hash_table_new_full(correlation_key_hash, correlation_key_equal, NULL,
                                      (GDestroyNotify) correlation_context_unref);
  self->timer_wheel = timer_wheel_new();
  cached_g_current_time(&self->last_tick);
}

void
correlation_state_deinit_instance(CorrelationState *self)
{
  if (self->state)
    g_hash_table_destroy(self->state);
  timer_wheel_free(self->timer_wheel);
  g_mutex_clear(&self->lock);
}

CorrelationState *
correlation_state_new(void)
{
  CorrelationState *self = g_new0(CorrelationState, 1);

  correlation_state_init_instance(self);
  return self;
}

void
correlation_state_free(CorrelationState *self)
{
  correlation_state_deinit_instance(self);
  g_free(self);
}
