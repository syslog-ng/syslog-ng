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

#ifndef HEALTHCHECK_H
#define HEALTHCHECK_H

#include "syslog-ng.h"

typedef struct _HealthCheck HealthCheck;

typedef struct _HealthCheckResult
{
  guint64 io_worker_latency;
  guint64 mainloop_io_worker_roundtrip_latency;
} HealthCheckResult;

typedef void(*HealthCheckCompletionCB)(HealthCheckResult, gpointer);

HealthCheck *healthcheck_new(void);
HealthCheck *healthcheck_ref(HealthCheck *self);
void healthcheck_unref(HealthCheck *self);

gboolean healthcheck_run(HealthCheck *self, HealthCheckCompletionCB completion, gpointer user_data);

#endif
