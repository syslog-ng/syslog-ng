/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  SC_TYPE_DROPPED=0, /* number of messages dropped */
  SC_TYPE_PROCESSED, /* number of messages processed */
  SC_TYPE_STORED,    /* number of messages on disk */
  SC_TYPE_SUPPRESSED,/* number of messages suppressed */
  SC_TYPE_STAMP,     /* timestamp */
  SC_TYPE_MAX
} StatsCounterType;

enum
{
  SCS_SOURCE         = 0x0100,
  SCS_DESTINATION    = 0x0200,
  SCS_FILE           = 1,
  SCS_PIPE           = 2,
  SCS_TCP            = 3,
  SCS_UDP            = 4,
  SCS_TCP6           = 5,
  SCS_UDP6           = 6,
  SCS_UNIX_STREAM    = 7,
  SCS_UNIX_DGRAM     = 8,
  SCS_SYSLOG         = 9,
  SCS_INTERNAL       = 10,
  SCS_LOGSTORE       = 11,
  SCS_PROGRAM        = 12,
  SCS_SQL            = 13,
  SCS_SUN_STREAMS    = 14,
  SCS_USERTTY        = 15,
  SCS_GROUP          = 16,
  SCS_CENTER         = 17,
  SCS_HOST           = 18,
  SCS_MAX,
  SCS_SOURCE_MASK    = 0xff
};

typedef struct _StatsCounter StatsCounter;


void stats_generate_log(void);
GString *stats_generate_csv(void);
void stats_register_counter(gint level, gint source, const gchar *id, const gchar *instance, StatsCounterType type, guint32 **counter);
StatsCounter *
stats_register_dynamic_counter(gint stats_level, gint source, const gchar *id, const gchar *instance, StatsCounterType type, guint32 **counter, gboolean *new);
void stats_register_associated_counter(StatsCounter *handle, StatsCounterType type, guint32 **counter);
void stats_unregister_counter(gint source, const gchar *id, const gchar *instance, StatsCounterType type, guint32 **counter);
void stats_unregister_dynamic_counter(StatsCounter *handle, StatsCounterType type, guint32 **counter);
void stats_cleanup_orphans(void);

void stats_init(void);
void stats_destroy(void);

static inline void
stats_counter_inc(guint32 *counter)
{
  if (counter)
    (*counter)++;

}
static inline void
stats_counter_dec(guint32 *counter)
{
  if (counter)
    (*counter)--;
}

static inline void
stats_counter_set(guint32 *counter, guint32 value)
{
  if (counter)
    *counter = value;
}

#endif

