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

#ifndef SDINTER_H_INCLUDED
#define SDINTER_H_INCLUDED

#include "driver.h"
#include "logsource.h"
#include "mainloop-worker.h"
#include "stats/stats-counter.h"

typedef struct AFInterSourceOptions
{
  LogSourceOptions super;
  gint queue_capacity;
} AFInterSourceOptions;

/*
 * This is the actual source driver, linked into the configuration tree.
 */
typedef struct _AFInterSourceDriver
{
  LogSrcDriver super;
  LogSource *source;
  AFInterSourceOptions source_options;
} AFInterSourceDriver;

typedef struct _AFInterMetrics
{
  StatsCounterItem *processed;
  StatsCounterItem *dropped;
  StatsCounterItem *queued;
  StatsCounterItem *queue_capacity;
} AFInterMetrics;

void afinter_postpone_mark(gint mark_freq);
LogDriver *afinter_sd_new(GlobalConfig *cfg);
void afinter_global_init(void);
void afinter_global_deinit(void);

AFInterMetrics afinter_get_metrics(void);

#endif
