/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "ml-batched-timer.h"
#include "mainloop-call.h"

/* callback to be invoked when the timeout triggers */
static void
ml_batched_timer_handle(MlBatchedTimer *self)
{
  self->handler(self->cookie);
}

/* function called using main_loop_call() in case the suppress timer needs
 * to be updated.  It is running in the main thread, thus is able to
 * reregister our ivykis timer */
static void
ml_batched_timer_perform_update(MlBatchedTimer *self)
{
  main_loop_assert_main_thread();

  if (iv_timer_registered(&self->timer))
    iv_timer_unregister(&self->timer);

  self->timer.expires = self->expires;

  if (self->timer.expires.tv_sec > 0)
    iv_timer_register(&self->timer);
  self->unref_cookie(self->cookie);
}

static inline gboolean
ml_batched_timer_expiration_changed(MlBatchedTimer *self, struct timespec *next_expires)
{
  return ((next_expires->tv_sec != self->expires.tv_sec) ||
          (next_expires->tv_nsec != self->expires.tv_nsec));
}

/*
 * Update the timer in a deferred manner, possibly batching the results of
 * multiple updates to the underlying ivykis timer.  This is necessary as
 * suppress timer updates must run in the main thread, and updating it every
 * time a new message comes in would cause enormous latency in the fast
 * path.  By collecting multiple updates the overhead is drastically
 * reduced.
 */
static void
ml_batched_timer_update(MlBatchedTimer *self, struct timespec *next_expires)
{

  /* NOTE: this check is racy as self->expires might be updated in a
   * different thread without holding a lock.
   *
   * When we lose the race, that means that another thread has already
   * updated the expires field, but we see the old value.  In this case two
   * things may happen:
   *
   *   1) we skip an update because of the race
   *
   *      We're going to skip the update if the other set the "expires" field to
   *      the same value we intended to set it.  This is not an issue, it doesn't
   *      matter whether we or the other thread updates the timer.
   *
   *   2) we perform an update because of the race
   *
   *      In this case, the other thread has updated the field, but we still
   *      see the old value, thus we decide another update is due.  We go
   *      into the locked path, which will sort things out.
   *
   * In both cases we are fine.
   */

  if (ml_batched_timer_expiration_changed(self, next_expires))
    {
      g_static_mutex_lock(&self->lock);

      /* check if we've lost the race */
      if (ml_batched_timer_expiration_changed(self, next_expires))
        {
          /* we need to update the timer */
          self->expires = *next_expires;
          self->ref_cookie(self->cookie);
          g_static_mutex_unlock(&self->lock);
          main_loop_call((MainLoopTaskFunc) ml_batched_timer_perform_update, self, FALSE);
        }
      else
        g_static_mutex_unlock(&self->lock);
    }
}

/* Update the expire time of this timer to the current time plus @sec. Can
 * be invoked from any threads. */
void
ml_batched_timer_postpone(MlBatchedTimer *self, glong sec)
{
  struct timespec next_expires;

  iv_validate_now();

  /* we deliberately use nsec == 0 in order to increase the likelihood that
   * we target the same second, in case only a fraction of a second has
   * passed between two updates.  */
  next_expires.tv_nsec = 0;
  next_expires.tv_sec = iv_now.tv_sec + sec;
  ml_batched_timer_update(self, &next_expires);
}

/* cancel the timer for the time being. Can be invoked from any threads. */
void
ml_batched_timer_cancel(MlBatchedTimer *self)
{
  struct timespec next_expires;

  next_expires.tv_sec = 0;
  next_expires.tv_nsec = 0;
  ml_batched_timer_update(self, &next_expires);
}

/* unregister the underlying ivykis timer, can only be called from the main thread. */
void
ml_batched_timer_unregister(MlBatchedTimer *self)
{
  main_loop_assert_main_thread();

  if (iv_timer_registered(&self->timer))
    iv_timer_unregister(&self->timer);
  self->expires.tv_sec = 0;
  self->expires.tv_nsec = 0;
}

/* one-time initialization of the MlBatchedTimer structure */
void
ml_batched_timer_init(MlBatchedTimer *self)
{
  g_static_mutex_init(&self->lock);
  IV_TIMER_INIT(&self->timer);
  self->timer.cookie = self;
  self->timer.handler = (void (*)(void *)) ml_batched_timer_handle;
}

/* Free MlBatchedTimer state. */
void
ml_batched_timer_free(MlBatchedTimer *self)
{
  g_static_mutex_free(&self->lock);
}
