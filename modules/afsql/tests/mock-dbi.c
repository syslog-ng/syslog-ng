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

#include "mock-dbi.h"
#include <stdlib.h>

dbi_result mock_dbi_query_result = (dbi_result) 0x1;
gchar *mock_dbi_last_query = NULL;
gboolean mock_dbi_result_freed = FALSE;
const gchar *mock_dbi_error_message = "mock dbi error";

void
mock_dbi_reset(void)
{
  mock_dbi_query_result = (dbi_result) 0x1;
  g_free(mock_dbi_last_query);
  mock_dbi_last_query = NULL;
  mock_dbi_result_freed = FALSE;
  mock_dbi_error_message = "mock dbi error";
}

/* --- mocked functions (state-bearing) --- */

dbi_result
dbi_conn_query(dbi_conn conn, const char *statement)
{
  g_free(mock_dbi_last_query);
  mock_dbi_last_query = g_strdup(statement);
  return mock_dbi_query_result;
}

int
dbi_conn_error(dbi_conn conn, const char **errmsg)
{
  if (errmsg)
    *errmsg = mock_dbi_error_message;
  return 0;
}

int
dbi_result_free(dbi_result result)
{
  mock_dbi_result_freed = TRUE;
  return 0;
}

size_t
dbi_conn_quote_string_copy(dbi_conn conn, const char *orig, char **dest)
{
  if (!orig || !dest)
    return 0;

  /* Minimal SQL quoting: wrap in single quotes, escape internal single quotes */
  GString *quoted = g_string_new("'");
  for (const char *s = orig; *s; s++)
    {
      if (*s == '\'')
        g_string_append(quoted, "\\'");
      else
        g_string_append_c(quoted, *s);
    }
  g_string_append_c(quoted, '\'');
  size_t quoted_len = quoted->len;
  *dest = g_string_free(quoted, FALSE);
  return quoted_len;
}

/* --- no-op stubs for the rest of afsql.c --- */

dbi_conn
dbi_conn_new_r(const char *name, dbi_inst inst)
{
  return NULL;
}

int
dbi_conn_connect(dbi_conn conn)
{
  return -1;
}

void
dbi_conn_close(dbi_conn conn)
{
}

int
dbi_conn_ping(dbi_conn conn)
{
  return 0;
}

int
dbi_conn_set_option(dbi_conn conn, const char *key, const char *value)
{
  return 0;
}

int
dbi_conn_set_option_numeric(dbi_conn conn, const char *key, int value)
{
  return 0;
}

size_t
dbi_conn_quote_binary_copy(dbi_conn conn, const unsigned char *orig, size_t length, unsigned char **dest)
{
  if (dest)
    *dest = NULL;
  return 0;
}

int
dbi_initialize_r(const char *driverdir, dbi_inst *pInst)
{
  return 0;
}

unsigned int
dbi_result_get_field_idx(dbi_result result, const char *fieldname)
{
  return 0;
}
