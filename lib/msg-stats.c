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
#include "msg-stats.h"
#include "syslog-names.h"
#include "logmsg/logmsg.h"
#include "apphook.h"

/* Static counters for severities and facilities */
/* LOG_DEBUG 0x7 */
#define SEVERITY_MAX   (0x7 + 1)
/* LOG_LOCAL7 23<<3, one additional slot for "everything-else" counter */
#define FACILITY_MAX   (23 + 1 + 1)

static StatsCounterItem *severity_counters[SEVERITY_MAX];
static StatsCounterItem *facility_counters[FACILITY_MAX];

static void
_process_message_pri(guint16 pri)
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
msg_stats_update_counters(const gchar *source_id, const LogMessage *msg)
{
  if (stats_check_level(2))
    {
      stats_lock();

      StatsClusterKey sc_key;
      stats_cluster_logpipe_key_set(&sc_key, "host", SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_HOST, NULL) );
      stats_register_and_increment_dynamic_counter(2, &sc_key, msg->timestamps[LM_TS_RECVD].ut_sec);

      if (stats_check_level(3))
        {
          stats_cluster_logpipe_key_set(&sc_key, "sender", SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_HOST_FROM, NULL) );
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].ut_sec);
          stats_cluster_logpipe_key_set(&sc_key, "program", SCS_SOURCE, NULL, log_msg_get_value(msg, LM_V_PROGRAM, NULL) );
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].ut_sec);

          stats_cluster_logpipe_key_set(&sc_key, "host", SCS_SOURCE, source_id, log_msg_get_value(msg, LM_V_HOST, NULL));
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].ut_sec);
          stats_cluster_logpipe_key_set(&sc_key, "sender", SCS_SOURCE, source_id, log_msg_get_value(msg, LM_V_HOST_FROM,
                                        NULL));
          stats_register_and_increment_dynamic_counter(3, &sc_key, msg->timestamps[LM_TS_RECVD].ut_sec);
        }

      stats_unlock();
    }
  _process_message_pri(msg->pri);
}

static void
stats_syslog_reinit(void)
{
  gchar name[11] = "";
  gint i;
  StatsClusterKey sc_key;

  stats_lock();
  if (stats_check_level(3))
    {
      /* we need these counters, register them */
      for (i = 0; i < SEVERITY_MAX; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_cluster_logpipe_key_set(&sc_key, "severity", SCS_SOURCE, NULL, name );
          stats_register_counter(3, &sc_key, SC_TYPE_PROCESSED, &severity_counters[i]);
        }

      for (i = 0; i < FACILITY_MAX - 1; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_cluster_logpipe_key_set(&sc_key, "facility", SCS_SOURCE, NULL, name );
          stats_register_counter(3, &sc_key, SC_TYPE_PROCESSED, &facility_counters[i]);
        }
      stats_cluster_logpipe_key_set(&sc_key, "facility", SCS_SOURCE, NULL, "other" );
      stats_register_counter(3, &sc_key, SC_TYPE_PROCESSED, &facility_counters[FACILITY_MAX - 1]);
    }
  else
    {
      /* no need for facility/severity counters, unregister them */
      for (i = 0; i < SEVERITY_MAX; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_cluster_logpipe_key_set(&sc_key, "severity", SCS_SOURCE, NULL, name );
          stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &severity_counters[i]);
        }

      for (i = 0; i < FACILITY_MAX - 1; i++)
        {
          g_snprintf(name, sizeof(name), "%d", i);
          stats_cluster_logpipe_key_set(&sc_key, "facility", SCS_SOURCE, NULL, name );
          stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &facility_counters[i]);
        }
      stats_cluster_logpipe_key_set(&sc_key, "facility", SCS_SOURCE, NULL, "other" );
      stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &facility_counters[FACILITY_MAX - 1]);
    }
  stats_unlock();
}

void
msg_stats_init(void)
{
  register_application_hook(AH_CONFIG_CHANGED, (ApplicationHookFunc) stats_syslog_reinit, NULL);
}

void
msg_stats_deinit(void)
{
}
