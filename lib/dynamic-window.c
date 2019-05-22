/*
 * Copyright (c) 2002-2019 One Identity
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@balabit.com>
 * Copyright (c) 2019 László Várady <laszlo.varady@balabit.com>
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

#include <syslog-ng.h>
#include "dynamic-window.h"

void
dynamic_window_stat_update(DynamicWindowStat *self, gsize value)
{
  self->sum += value;
  self->n++;
}

void
dynamic_window_stat_reset(DynamicWindowStat *self)
{
  self->sum = 0;
  self->n = 0;
}

gsize
dynamic_window_stat_get_avg(DynamicWindowStat *self)
{
  if (self->n == 0)
    return 0;

  return self->sum / self->n;
}

gsize
dynamic_window_stat_get_number_of_samples(DynamicWindowStat *self)
{
  return self->n;
}

gsize
dynamic_window_stat_get_sum(DynamicWindowStat *self)
{
  return self->sum;
}

void
dynamic_window_set_counter(DynamicWindow *self, DynamicWindowCounter *ctr)
{
  self->window_ctr = ctr;
  dynamic_window_stat_reset(&self->stat);
}

gsize
dynamic_window_request(DynamicWindow *self, gsize size)
{
  if (!self->window_ctr)
    return 0;

  return dynamic_window_counter_request(self->window_ctr, size);
}

void
dynamic_window_release(DynamicWindow *self, gsize size)
{
  if (!self->window_ctr)
    return;

  dynamic_window_counter_release(self->window_ctr, size);
}
