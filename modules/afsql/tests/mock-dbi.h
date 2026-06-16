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

#ifndef MOCK_DBI_H_INCLUDED
#define MOCK_DBI_H_INCLUDED

#pragma GCC diagnostic ignored "-Wstrict-prototypes"

#include <dbi.h>
#include <glib.h>

/*
 * Mock state controlling dbi_conn_query behaviour.
 */
extern dbi_result mock_dbi_query_result;
extern gchar *mock_dbi_last_query;
extern gboolean mock_dbi_result_freed;
extern const gchar *mock_dbi_error_message;

void mock_dbi_reset(void);

#endif /* MOCK_DBI_H_INCLUDED */
