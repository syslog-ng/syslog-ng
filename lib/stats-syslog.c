/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "stats-syslog.h"
#include "syslog-names.h"


/* Static counters for severities and facilities */
/* LOG_DEBUG 0x7 */
#define SEVERITY_MAX   (0x7 + 1)
/* LOG_LOCAL7 23<<3, one additional slot for "everything-else" counter */
#define FACILITY_MAX   (23 + 1 + 1)

static StatsCounterItem *severity_counters[SEVERITY_MAX];
static StatsCounterItem *facility_counters[FACILITY_MAX];

void
stats_syslog_process_message_pri(guint16 pri)
{
  int lpri = LOG_FAC(pri);

  stats_counter_inc(severity_counters[LOG_PRI(pri)]);
  if (lpri > (FACILITY_MAX - 1))
    {
      /* the large facilities (=facility.other) are collected in the last array item */
      lpri = FACILITY_MAX - 1;
    }
  stats_counter_inc(facility_counters[lpri]);
}

void
stats_syslog_reinit(void)
{
  gchar name[11] = "";
  gint i;

  stats_lock();
  if (stats_check_level(3))
    {
      /* we need these counters, register them */
      for (i = 0; i < SEVERITY_MAX; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_register_counter(3, SCS_SEVERITY | SCS_SOURCE, NULL, name, SC_TYPE_PROCESSED, &severity_counters[i]);
        }

      for (i = 0; i < FACILITY_MAX - 1; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_register_counter(3, SCS_FACILITY | SCS_SOURCE, NULL, name, SC_TYPE_PROCESSED, &facility_counters[i]);
        }
      stats_register_counter(3, SCS_FACILITY | SCS_SOURCE, NULL, "other", SC_TYPE_PROCESSED, &facility_counters[FACILITY_MAX - 1]);
    }
  else
    {
      /* no need for facility/severity counters, unregister them */
      for (i = 0; i < SEVERITY_MAX; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_unregister_counter(SCS_SEVERITY | SCS_SOURCE, NULL, name, SC_TYPE_PROCESSED, &severity_counters[i]);
        }

      for (i = 0; i < FACILITY_MAX - 1; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_unregister_counter(SCS_FACILITY | SCS_SOURCE, NULL, name, SC_TYPE_PROCESSED, &facility_counters[i]);
        }
      stats_unregister_counter(SCS_FACILITY | SCS_SOURCE, NULL, "other", SC_TYPE_PROCESSED, &facility_counters[FACILITY_MAX - 1]);
    }
  stats_unlock();
}
