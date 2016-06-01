/*
 * Copyright (c) 2010-2016 Balabit
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

#include "afmongodb-legacy-grammar.h"
#include "afmongodb-legacy-private.h"

gboolean
afmongodb_dd_validate_socket_combination(LogDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if ((self->port != 0 || self->port != MONGO_CONN_LOCAL) && self->address != NULL)
    return FALSE;
  if (self->servers)
    return FALSE;

  return TRUE;
}

gboolean
afmongodb_dd_validate_network_combination(LogDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if (self->port == MONGO_CONN_LOCAL && self->address != NULL)
    return FALSE;

  return TRUE;
}

void
afmongodb_dd_set_user(LogDriver *d, const gchar *user)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once("WARNING: Using username() option is deprecated in mongodb driver,"
      " please use uri() instead");

  g_free(self->user);
  self->user = g_strdup(user);
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_password(LogDriver *d, const gchar *password)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once("WARNING: Using password() option is deprecated in mongodb driver,"
      " please use uri() instead");

  g_free(self->password);
  self->password = g_strdup(password);
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_host(LogDriver *d, const gchar *host)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once(
      "WARNING: Using host() option is deprecated in mongodb driver, please use uri() instead");

  g_free(self->address);
  self->address = g_strdup(host);
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_port(LogDriver *d, gint port)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once(
      "WARNING: Using port() option is deprecated in mongodb driver, please use uri() instead");

  self->port = port;
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_servers(LogDriver *d, GList *servers)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once(
      "WARNING: Using servers() option is deprecated in mongodb driver, please use uri() instead");

  string_list_free(self->servers);
  self->servers = servers;
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_path(LogDriver *d, const gchar *path)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once(
      "WARNING: Using path() option is deprecated in mongodb driver, please use uri() instead");

  g_free(self->address);
  self->address = g_strdup(path);
  self->port = MONGO_CONN_LOCAL;
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_database(LogDriver *d, const gchar *database)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once("WARNING: Using database() option is deprecated in mongodb driver,"
      " please use uri() instead");

  g_free(self->db);
  self->db = g_strdup(database);
  self->is_legacy = TRUE;
}

void
afmongodb_dd_set_safe_mode(LogDriver *d, gboolean state)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once("WARNING: Using safe_mode() option is deprecated in mongodb driver,"
      " please use uri() instead");

  self->safe_mode = state;
  self->is_legacy = TRUE;
}
