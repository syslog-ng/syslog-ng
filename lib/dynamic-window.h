/*
 * Copyright (c) 2002-2019 One Identity
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@balabit.com>
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

#ifndef DYNAMIC_WINDOW_H_INCLUDED
#define DYNAMIC_WINDOW_H_INCLUDED

#include "syslog-ng.h"
#include "dynamic-window-pool.h"

typedef struct _DynamicWindow DynamicWindow;

typedef struct _DynamicWindowStat DynamicWindowStat;

struct _DynamicWindowStat
{
  gsize n;
  guint64 sum;
};

void dynamic_window_stat_update(DynamicWindowStat *self, gsize value);
void dynamic_window_stat_reset(DynamicWindowStat *self);
gsize dynamic_window_stat_get_avg(DynamicWindowStat *self);
gsize dynamic_window_stat_get_number_of_samples(DynamicWindowStat *self);
guint64 dynamic_window_stat_get_sum(DynamicWindowStat *self);

struct _DynamicWindow
{
  DynamicWindowPool *pool;
  DynamicWindowStat stat;
};

void dynamic_window_set_pool(DynamicWindow *self, DynamicWindowPool *pool);
gboolean dynamic_window_is_enabled(DynamicWindow *self);
gsize dynamic_window_request(DynamicWindow *self, gsize size);
void dynamic_window_release(DynamicWindow *self, gsize size);

#endif
