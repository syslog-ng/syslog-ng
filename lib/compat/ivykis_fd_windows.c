/*
 * Copyright (c) 2025 One Identity
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

#ifdef _WIN32

#include "compat/ivykis_fd.h"
#include <iv.h>
#include <iv_timer.h>
#include <iv_task.h>
#include <stdlib.h>
#include <string.h>

struct win_impl
{
  int registered;
  struct iv_timer tick;
  struct iv_task  immed;
};

static void tick_cb(void *cookie)
{
  struct compat_iv_fd *w = (struct compat_iv_fd *)cookie;
  /* call out if handlers are set; you can refine readiness checks later */
  if (w->handler_in)  w->handler_in(w->cookie);
  if (w->handler_out) w->handler_out(w->cookie);
  if (w->handler_err) { /* no explicit ERR trigger here; keep for future */ }
  /* rearm timer for a short interval */
  iv_validate_now();
  ((struct win_impl *)w->impl)->tick.expires = iv_now;
  ((struct win_impl *)w->impl)->tick.expires.tv_sec  += 0;
  ((struct win_impl *)w->impl)->tick.expires.tv_nsec += 50 * 1000 * 1000; /* 50ms */
  iv_timer_register(&((struct win_impl *)w->impl)->tick);
}

void compat_iv_fd_init(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)malloc(sizeof(*impl));
  memset(impl, 0, sizeof(*impl));
  w->impl = impl;
  IV_TIMER_INIT(&impl->tick);
  impl->tick.cookie  = w;
  impl->tick.handler = tick_cb;

  IV_TASK_INIT(&impl->immed);
  impl->immed.cookie  = w;
  impl->immed.handler = NULL; /* not used yet */
}

void compat_iv_fd_set_handler_in (struct compat_iv_fd *w, void (*fn)(void *))
{
  w->handler_in  = fn;
}
void compat_iv_fd_set_handler_out(struct compat_iv_fd *w, void (*fn)(void *))
{
  w->handler_out = fn;
}
void compat_iv_fd_set_handler_err(struct compat_iv_fd *w, void (*fn)(void *))
{
  w->handler_err = fn;
}

int  compat_iv_fd_register_try(struct compat_iv_fd *w)
{
  /* Not pollable on Windows: don't arm our timer; let logwriter use its
     non-pollable (task/timer) path. */
  ((struct win_impl *)w->impl)->registered = 0;
  return -1;
}

void compat_iv_fd_register(struct compat_iv_fd *w)
{
  ((struct win_impl *)w->impl)->registered = 1;
  tick_cb(w);
}

void compat_iv_fd_unregister(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)w->impl;
  if (iv_timer_registered(&impl->tick)) iv_timer_unregister(&impl->tick);
  ((struct win_impl *)w->impl)->registered = 0;
}

int  compat_iv_fd_registered(struct compat_iv_fd *w)
{
  return ((struct win_impl *)w->impl)->registered;
}

void compat_iv_fd_deinit(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)w->impl;
  if (!impl) return;
  if (iv_timer_registered(&impl->tick)) iv_timer_unregister(&impl->tick);
  free(impl);
  w->impl = NULL;
}

#endif