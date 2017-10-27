/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
  
#ifndef STATS_H_INCLUDED
#define STATS_H_INCLUDED

#include "syslog-ng.h"
#include "stats/stats-cluster.h"

typedef struct _StatsOptions
{
  gint log_freq;
  gint level;
  gint lifetime;
  gint max_dynamic;
} StatsOptions;

enum
{
  STATS_LEVEL0 = 0,
  STATS_LEVEL1,
  STATS_LEVEL2,
  STATS_LEVEL3
};

void stats_reinit(StatsOptions *options);
void stats_init(void);
void stats_destroy(void);

void stats_options_defaults(StatsOptions *options);

#endif

