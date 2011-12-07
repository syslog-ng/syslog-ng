/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#ifndef AFSQL_H_INCLUDED
#define AFSQL_H_INCLUDED

#include "driver.h"

enum
{
  AFSQL_COLUMN_DEFAULT = 1,
};

#if ENABLE_SQL

void afsql_dd_set_type(LogDriver *s, const gchar *type);
void afsql_dd_set_host(LogDriver *s, const gchar *host);
gboolean afsql_dd_check_port(const gchar *port);
void afsql_dd_set_port(LogDriver *s, const gchar *port);
void afsql_dd_set_user(LogDriver *s, const gchar *user);
void afsql_dd_set_password(LogDriver *s, const gchar *password);
void afsql_dd_set_database(LogDriver *s, const gchar *database);
void afsql_dd_set_table(LogDriver *s, const gchar *table);
void afsql_dd_set_columns(LogDriver *s, GList *columns);
void afsql_dd_set_values(LogDriver *s, GList *values);
void afsql_dd_set_null_value(LogDriver *s, const gchar *null);
void afsql_dd_set_indexes(LogDriver *s, GList *indexes);
void afsql_dd_set_retries(LogDriver *s, gint num_retries);
void afsql_dd_set_frac_digits(LogDriver *s, gint frac_digits);
void afsql_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone);
void afsql_dd_set_send_time_zone(LogDriver *s, const gchar *send_time_zone);
void afsql_dd_set_flush_lines(LogDriver *s, gint flush_lines);
void afsql_dd_set_flush_timeout(LogDriver *s, gint flush_timeout);
void afsql_dd_set_session_statements(LogDriver *s, GList *session_statements);
void afsql_dd_set_flags(LogDriver *s, gint flags);
LogDriver *afsql_dd_new();
gint afsql_dd_lookup_flag(const gchar *flag);
void afsql_dd_set_retries(LogDriver *s, gint num_retries);
void afsql_dd_add_dbd_option(LogDriver *s, const gchar *name, const gchar *value);
void afsql_dd_add_dbd_option_numeric(LogDriver *s, const gchar *name, gint value);

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
#define afsql_dd_set_null_value(s, v)
#define afsql_dd_add_dbd_option(s, n, v)
#define afsql_dd_add_dbd_option_numeric(s, n, v)

#define afsql_dd_new() 0

#endif

#endif
