/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef AFSQL_H_INCLUDED
#define AFSQL_H_INCLUDED

#include "driver.h"

#if ENABLE_SQL

void afsql_dd_set_type(LogDriver *s, const gchar *type);
void afsql_dd_set_host(LogDriver *s, const gchar *host);
void afsql_dd_set_port(LogDriver *s, const gchar *host);
void afsql_dd_set_user(LogDriver *s, const gchar *user);
void afsql_dd_set_password(LogDriver *s, const gchar *password);
void afsql_dd_set_database(LogDriver *s, const gchar *database);
void afsql_dd_set_table(LogDriver *s, const gchar *table);
void afsql_dd_set_columns(LogDriver *s, GList *columns);
void afsql_dd_set_values(LogDriver *s, GList *values);
void afsql_dd_set_indexes(LogDriver *s, GList *indexes);
void afsql_dd_set_mem_fifo_size(LogDriver *s, gint mem_fifo_size);
void afsql_dd_set_disk_fifo_size(LogDriver *s, gint64 disk_fifo_size);
void afsql_dd_set_frac_digits(LogDriver *s, gint frac_digits);
void afsql_dd_set_time_zone_string(LogDriver *s, const gchar *time_zone_string);
LogDriver *afsql_dd_new();

#else

#define afsql_dd_set_type(s, t)
#define afsql_dd_set_host(s, h)
#define afsql_dd_set_port(s, p)
#define afsql_dd_set_user(s, u)
#define afsql_dd_set_password(s, p)
#define afsql_dd_set_database(s, d)
#define afsql_dd_set_table(s, t)
#define afsql_dd_set_columns(s, c)
#define afsql_dd_set_values(s, v)
#define afsql_dd_set_mem_fifo_size(s, f)
#define afsql_dd_set_disk_fifo_size(s, f)

#define afsql_dd_new() 0

#endif

#endif

