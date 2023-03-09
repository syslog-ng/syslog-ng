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

static void
_send_healthcheck_reply(HealthCheckResult result, gpointer c)
{
  ControlConnection *cc = (ControlConnection *) c;

  GString *reply = g_string_new("OK ");

  g_string_append_printf(reply, PROMETHEUS_METRIC_PREFIX
                         "io_worker_latency_nanoseconds %" G_GUINT64_FORMAT"\n",
                         result.io_worker_latency);
  g_string_append_printf(reply, PROMETHEUS_METRIC_PREFIX
                         "mainloop_io_worker_roundtrip_latency_nanoseconds %" G_GUINT64_FORMAT"\n",
                         result.mainloop_io_worker_roundtrip_latency);

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
