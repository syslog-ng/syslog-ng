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

#ifndef AFSQL_TEST_HELPERS_H_INCLUDED
#define AFSQL_TEST_HELPERS_H_INCLUDED

#include "test_afsql.h"
#include "mock-dbi.h"
#include "cfg.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"

static inline AFSqlDestDriver *
_create_driver(void)
{
  AFSqlDestDriver *self = g_new0(AFSqlDestDriver, 1);
  self->type            = g_strdup("mysql");
  self->host            = g_strdup("localhost");
  self->port            = g_strdup("3306");
  self->user            = g_strdup("testuser");
  self->database        = g_strdup("testdb");
  self->quote_as_string = g_strdup("");
  log_template_options_defaults(&self->template_options);
  return self;
}

static inline void
_free_driver(AFSqlDestDriver *self)
{
  for (gint i = 0; i < self->fields_len; i++)
    {
      g_free(self->fields[i].name);
      g_free(self->fields[i].type);
      log_template_unref(self->fields[i].value);
    }
  g_free(self->fields);
  g_free(self->type);
  g_free(self->host);
  g_free(self->port);
  g_free(self->user);
  g_free(self->database);
  g_free(self->quote_as_string);
  g_free(self->null_value);
  log_template_unref(self->table);
  g_free(self);
}

static inline void
_set_fields(AFSqlDestDriver *self, const gchar **col_names,
            const gchar **templates, gint count)
{
  self->fields_len = count;
  self->fields = g_new0(AFSqlField, count);
  for (gint i = 0; i < count; i++)
    {
      self->fields[i].name  = g_strdup(col_names[i]);
      self->fields[i].type  = g_strdup("text");
      self->fields[i].value = log_template_new(configuration, NULL);
      log_template_compile(self->fields[i].value, templates[i], NULL);
    }
}

static inline GString *
_make_table(const gchar *name)
{
  return g_string_new(name);
}

#endif /* AFSQL_TEST_HELPERS_H_INCLUDED */
