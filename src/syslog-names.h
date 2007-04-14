/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SYSLOG_NAMES_H_INCLUDED
#define __SYSLOG_NAMES_H_INCLUDED

#include "syslog-ng.h"
#include <syslog.h>

struct sl_name
{
  char *name;
  int value;
};

extern struct sl_name sl_levels[];
extern struct sl_name sl_facilities[];

/* returns an index where this name is found */
int syslog_name_lookup_id_by_name(const char *name, struct sl_name names[]);
int syslog_name_lookup_value_by_name(const char *name, struct sl_name names[]);
const char *syslog_name_lookup_name_by_value(int value, struct sl_name names[]);

guint32 syslog_make_range(guint32 r1, guint32 r2);

#define syslog_name_lookup_level_by_name(name) syslog_name_lookup_value_by_name(name, sl_levels)
#define syslog_name_lookup_facility_by_name(name) syslog_name_lookup_id_by_name(name, sl_facilities)

#endif
