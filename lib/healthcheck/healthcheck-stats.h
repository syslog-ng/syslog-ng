/*
 * Copyright (c) 2023 László Várady <laszlo.varady93@gmail.com>
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

#ifndef HEALTHCHECK_STATS_H
#define HEALTHCHECK_STATS_H

#include "syslog-ng.h"

typedef struct _HealthCheckStatsOptions
{
  gint freq;
} HealthCheckStatsOptions;

static inline void
healthcheck_stats_options_defaults(HealthCheckStatsOptions *options)
{
  options->freq = 300;
}

void healthcheck_stats_init(HealthCheckStatsOptions *options);
void healthcheck_stats_deinit(void);

void healthcheck_stats_global_init(void);

#endif
