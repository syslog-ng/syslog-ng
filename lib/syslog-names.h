/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef __SYSLOG_NAMES_H_INCLUDED
#define __SYSLOG_NAMES_H_INCLUDED

#include "syslog-ng.h"

#define LOG_FACMASK 0x03f8  /* mask to extract facility part */
/* facility of pri */
#define LOG_FAC(p)  (((p) & LOG_FACMASK) >> 3)
#ifndef LOG_PRIMASK
#define LOG_PRIMASK 0x07  /* mask to extract priority part (internal) */
#endif
/* extract priority */
#define LOG_PRI(p)  ((p) & LOG_PRIMASK)

struct sl_name
{
  const char *name;
  int value;
};

extern struct sl_name sl_levels[];
extern struct sl_name sl_facilities[];

/* returns an index where this name is found */
int syslog_name_lookup_id_by_name(const char *name, struct sl_name names[]);
int syslog_name_lookup_value_by_name(const char *name, struct sl_name names[]);
const char *syslog_name_lookup_name_by_value(int value, struct sl_name names[]);

guint32 syslog_make_range(guint32 r1, guint32 r2);

static inline guint32
syslog_name_lookup_level_by_name(const gchar *name)
{
  return syslog_name_lookup_value_by_name(name, sl_levels);
}

static inline guint32
syslog_name_lookup_facility_by_name(const gchar *name)
{
  return syslog_name_lookup_value_by_name(name, sl_facilities);
}

#endif
