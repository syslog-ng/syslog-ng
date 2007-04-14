/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef STATS_H_INCLUDED
#define STATS_H_INCLUDED

#include "syslog-ng.h"

typedef enum
{
  SC_TYPE_MIN,
  SC_TYPE_DROPPED=0,
  SC_TYPE_PROCESSED,
  SC_TYPE_MAX
} StatsCounterType;

void stats_generate_log(void);
void stats_register_counter(StatsCounterType type, const gchar *counter_name, guint32 **counter, gboolean shared);
void stats_unregister_counter(StatsCounterType type, const gchar *name, guint32 **counter);
void stats_orphan_counter(StatsCounterType type, const gchar *counter_name, guint32 **counter);
void stats_cleanup_orphans(void);

#endif

