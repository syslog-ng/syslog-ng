/*
 * Copyright (c) 2009-2013 Balabit
 * Copyright (c) 2009 Marton Illes
 * Copyright (c) 2009-2013 Balázs Scheidler
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

#ifndef TAGS_H_INCLUDED
#define TAGS_H_INCLUDED

#include "syslog-ng.h"

typedef guint16 LogTagId;

/* this is limited by the LogMessage structure, where a guint8 stores the
 * number of 32 bit ints, used to store the tags as a bitfield.  256 * 32 =
 * 8192.
 */

#if GLIB_SIZEOF_LONG == 4
#define LOG_TAGS_MAX   8192
#else
#define LOG_TAGS_MAX   16384
#endif

#define LOG_TAGS_UNDEF  0xFFFF

LogTagId log_tags_get_by_name(const gchar *name);
const gchar *log_tags_get_by_id(LogTagId id);
void log_tags_register_predefined_tag(const gchar *name, LogTagId id);

void log_tags_reinit_stats(void);
void log_tags_global_init(void);
void log_tags_global_deinit(void);

void log_tags_inc_counter(LogTagId id);
void log_tags_dec_counter(LogTagId id);

#endif
