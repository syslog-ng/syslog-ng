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

#include "healthcheck-control.h"
#include "healthcheck.h"

#include "control/control.h"
#include "control/control-connection.h"
#include "control/control-commands.h"
#include "stats/stats-prometheus.h"
#include "afinter.h"

static inline void
_append_internal_src_queue_metrics(AFInterMetrics metrics, GString *reply)
{
  gchar double_buf[G_ASCII_DTOSTR_BUF_SIZE];

  gdouble queued = (gint) stats_counter_get(metrics.queued);
  gsize queue_capacity = stats_counter_get(metrics.queue_capacity);

  gdouble internal_queue_usage = 1;

  if (queue_capacity != 0)
    internal_queue_usage = queued / queue_capacity;

  g_string_append_printf(reply, PROMETHEUS_METRIC_PREFIX
                         "internal_events_queue_usage_ratio %s\n",
                         g_ascii_dtostr(double_buf, G_N_ELEMENTS(double_buf), internal_queue_usage));
}

static void
_send_healthcheck_reply(HealthCheckResult result, gpointer c)
{
  ControlConnection *cc = (ControlConnection *) c;
  gchar double_buf[G_ASCII_DTOSTR_BUF_SIZE];

  gdouble io_worker_latency = result.io_worker_latency / 1e9;
  gdouble mainloop_io_worker_roundtrip_latency = result.mainloop_io_worker_roundtrip_latency / 1e9;

  GString *reply = g_string_new("OK ");
  g_string_append_printf(reply, PROMETHEUS_METRIC_PREFIX
                         "io_worker_latency_seconds %s\n",
                         g_ascii_dtostr(double_buf, G_N_ELEMENTS(double_buf), io_worker_latency));
  g_string_append_printf(reply, PROMETHEUS_METRIC_PREFIX
                         "mainloop_io_worker_roundtrip_latency_seconds %s\n",
                         g_ascii_dtostr(double_buf, G_N_ELEMENTS(double_buf), mainloop_io_worker_roundtrip_latency));

  AFInterMetrics internal_src_metrics = afinter_get_metrics();
  if (internal_src_metrics.queued)
    _append_internal_src_queue_metrics(internal_src_metrics, reply);

  control_connection_send_reply(cc, reply);
  control_connection_unref(cc);
}

static void
control_connection_healthcheck(ControlConnection *cc, GString *command, gpointer user_data, gboolean *cancelled)
{
  HealthCheck *hc = healthcheck_new();

  if (!healthcheck_run(hc, _send_healthcheck_reply, control_connection_ref(cc)))
    {
      GString *reply = g_string_new("FAIL Another healthcheck command is already running");
      control_connection_send_reply(cc, reply);
      control_connection_unref(cc);
    }

  healthcheck_unref(hc);
}

void
healthcheck_register_control_commands(void)
{
  control_register_command("HEALTHCHECK", control_connection_healthcheck, NULL, FALSE);
}
