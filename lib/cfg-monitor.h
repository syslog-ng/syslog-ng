/*
 * Copyright (c) 2023 László Várady
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

#ifndef CFG_MONITOR_H
#define CFG_MONITOR_H

#include "syslog-ng.h"
#include <sys/stat.h>

typedef struct _CfgMonitor CfgMonitor;

typedef struct _CfgMonitorEvent
{
  const gchar *name;
  struct stat st;
  enum
  {
    MODIFIED
  } event;
} CfgMonitorEvent;

typedef void (*CfgMonitorEventCB)(const CfgMonitorEvent *event, gpointer c);

CfgMonitor *cfg_monitor_new(void);
void cfg_monitor_free(CfgMonitor *self);

void cfg_monitor_add_watch(CfgMonitor *self, CfgMonitorEventCB cb,  gpointer cb_data);
void cfg_monitor_remove_watch(CfgMonitor *self, CfgMonitorEventCB cb, gpointer cb_data);

void cfg_monitor_start(CfgMonitor *self);
void cfg_monitor_stop(CfgMonitor *self);

#endif
