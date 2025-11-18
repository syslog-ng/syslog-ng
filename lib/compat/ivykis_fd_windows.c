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
#include <stdlib.h>
#include <string.h>

struct win_impl
{
  int registered;
};

void compat_iv_fd_init(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)malloc(sizeof(*impl));
  if (!impl)
    return;
  memset(impl, 0, sizeof(*impl));
  w->impl = impl;
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
}

void compat_iv_fd_unregister(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)w->impl;
  if (!impl)
    return;
  impl->registered = 0;
}

int  compat_iv_fd_registered(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)w->impl;
  return impl ? impl->registered : 0;
}

void compat_iv_fd_deinit(struct compat_iv_fd *w)
{
  struct win_impl *impl = (struct win_impl *)w->impl;
  if (!impl)
    return;
  free(impl);
  w->impl = NULL;
}

#endif