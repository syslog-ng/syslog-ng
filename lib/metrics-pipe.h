/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "syslog-ng.h"
#include "logpipe.h"

typedef struct _MetricsPipe MetricsPipe;
struct _MetricsPipe
{
  LogPipe super;
  gchar *log_path_name;

  StatsCounterItem *ingress_counter;
  StatsCounterItem *egress_counter;
};

MetricsPipe *metrics_pipe_new(GlobalConfig *cfg, const gchar *log_path_name);
