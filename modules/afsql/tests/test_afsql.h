/*
 * Copyright (c) 2026 One Identity LLC.
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

#ifndef TEST_AFSQL_H_INCLUDED
#define TEST_AFSQL_H_INCLUDED

#include "afsql.h"

gboolean afsql_dd_run_query(AFSqlDestDriver *self, const gchar *query, gboolean silent,
                            dbi_result *result);
GString *afsql_dd_build_insert_command(AFSqlDestDriver *self, LogMessage *msg, GString *table);
GString *afsql_dd_ensure_accessible_database_table(AFSqlDestDriver *self, LogMessage *msg);

#endif /* TEST_AFSQL_H_INCLUDED */
