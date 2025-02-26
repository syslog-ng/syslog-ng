/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2024 Balázs Scheidler <balazs.scheidler@axoflow.com>
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
#ifndef SYSLOG_NG_CONSOLE_H_INCLUDED
#define SYSLOG_NG_CONSOLE_H_INCLUDED

#include "syslog-ng.h"

void console_printf(const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));

gboolean console_is_initial(void);
gboolean console_acquire_from_fds(gint fds[3], gint fds_to_steel);
void console_release(void);

void console_global_init(const gchar *console_prefix);
void console_global_deinit(void);

#endif
