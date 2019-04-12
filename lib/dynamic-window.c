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
dynamic_window_set_counter(DynamicWindow *self, DynamicWindowCounter *ctr)
{
  self->window_ctr = ctr;
  dynamic_window_stat_reset(self);
}

void
dynamic_window_stat_update(DynamicWindow *self, gsize window)
{
  self->window_stat.sum += window;
  self->window_stat.n++;
}

void
dynamic_window_stat_reset(DynamicWindow *self)
{
  self->window_stat.sum = 0;
  self->window_stat.n = 0;
}

gsize
dynamic_window_stat_get_avg(DynamicWindow *self)
{
  if (self->window_stat.n == 0)
    return 0;

  return self->window_stat.sum / self->window_stat.n;
}

gsize
dynamic_window_stat_get_number_of_samples(DynamicWindow *self)
{
  return self->window_stat.n;
}

gsize
dynamic_window_stat_get_sum(DynamicWindow *self)
{
  return self->window_stat.sum;
}
