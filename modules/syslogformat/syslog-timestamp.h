/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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

#ifndef SYSLOG_TIMESTAMP_H_INCLUDED
#define SYSLOG_TIMESTAMP_H_INCLUDED

#include "syslog-ng.h"
#include "logstamp.h"

gboolean log_msg_parse_rfc3164_date_unnormalized(LogStamp *stamp, const guchar **data, gint *length, struct tm *tm);
gboolean log_msg_parse_rfc5424_date_unnormalized(LogMessage *self, const guchar **data, gint *length, struct tm *tm);

#endif
