/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 2018 Laszlo Budai <laszlo.budai@balabit.com>
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

#include "window-size-counter.h"

#define COUNTER_MASK (((gsize)1<<(8*sizeof(gsize)-1)) - 1)
static const gsize COUNTER_MAX = COUNTER_MASK;
static const gsize SUSPEND_MASK = G_MAXSIZE ^ COUNTER_MASK;
static const gsize RESUME_MASK = G_MAXSIZE >> 1;

static gboolean
_is_suspended(gsize v)
{
  return (v == 0) || ((v & SUSPEND_MASK) == SUSPEND_MASK);
}

gsize
window_size_counter_get_max(void)
{
  return COUNTER_MAX;
}

void
window_size_counter_set(WindowSizeCounter *c, gsize value)
{
  atomic_gssize_set(&c->counter, value & COUNTER_MASK);
}

gsize
window_size_counter_get(WindowSizeCounter *c, gboolean *suspended)
{
  gsize v = atomic_gssize_get_unsigned(&c->counter);
  if (suspended)
    *suspended = _is_suspended(v);
  return v & COUNTER_MASK;
}

gsize
window_size_counter_add(WindowSizeCounter *c, gsize value, gboolean *suspended)
{
  gsize v = (gsize)atomic_gssize_add(&c->counter, value);
  gsize old_value = v & COUNTER_MASK;
  g_assert (old_value + value <= COUNTER_MAX);
  if (suspended)
    *suspended = _is_suspended(v);

  return old_value;
}

gsize
window_size_counter_sub(WindowSizeCounter *c, gsize value, gboolean *suspended)
{
  gsize v = (gsize)atomic_gssize_add(&c->counter, -1 * value);
  gsize old_value = v & COUNTER_MASK;
  g_assert (old_value >= value);
  if (suspended)
    *suspended = _is_suspended(v);

  return old_value;
}

void
window_size_counter_suspend(WindowSizeCounter *c)
{
  atomic_gssize_or(&c->counter, SUSPEND_MASK);
}

void
window_size_counter_resume(WindowSizeCounter *c)
{
  atomic_gssize_and(&c->counter, RESUME_MASK);
}

gboolean
window_size_counter_suspended(WindowSizeCounter *c)
{
  gsize v = atomic_gssize_get_unsigned(&c->counter);
  return _is_suspended(v);
}

