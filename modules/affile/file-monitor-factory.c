/*
 * Copyright (c) 2025 One Identity
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "file-monitor-factory.h"
#include "poll-fd-events.h"

#include <string.h>
#include <stdio.h>

FollowMethod
file_monitor_factory_follow_method_from_string(const gchar *method)
{
  if (strcmp(method, "auto") == 0)
    {
      return FM_AUTO;
    }
  else if (strcmp(method, "poll") == 0)
    {
      return FM_POLL;
    }
  else if (strcmp(method, "system") == 0)
    {
      return FM_SYSTEM_POLL;
    }
#if SYSLOG_NG_HAVE_INOTIFY
  else if (strcmp(method, "inotify") == 0)
    {
      return FM_INOTIFY;
    }
#endif
  return FM_UNKNOWN;
}
