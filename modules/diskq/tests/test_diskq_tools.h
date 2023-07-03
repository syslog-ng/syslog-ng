/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#ifndef TEST_DISKQ_TOOLS_H_
#define TEST_DISKQ_TOOLS_H_

#include "syslog-ng.h"
#include "logmsg/logmsg-serialize.h"
#include "diskq-options.h"

static inline void
_construct_options(DiskQueueOptions *options, guint64 capacity, gint flow_control_window, gboolean reliable)
{
  memset(options, 0, sizeof(DiskQueueOptions));
  options->capacity_bytes = capacity;
  options->flow_control_window_size = flow_control_window;
  options->flow_control_window_bytes = flow_control_window;
  options->front_cache_size = 0;
  options->reliable = reliable;
}


#endif /* TEST_DISKQ_TOOLS_H_ */
