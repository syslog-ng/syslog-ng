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

#include "afmongodb-legacy-uri.h"
#include "afmongodb-legacy-private.h"
#include "host-list.h"

#define DEFAULTHOST "127.0.0.1"
#define SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS 60000

static gboolean
_parse_addr(const char *str, char **host, gint *port)
{
  if (!host || !port)
    {
      msg_debug("Host or port reference should not be NULL");
      return FALSE;
    }
  char *proto_str = g_strdup_printf("mongodb://%s", str);
  mongoc_uri_t *uri = mongoc_uri_new(proto_str);
  if (!uri)
    {
      msg_error("Cannot parse MongoDB URI",
                evt_tag_str("uri", proto_str));
      g_free(proto_str);
      return FALSE;
    }

  const mongoc_host_list_t *hosts = mongoc_uri_get_hosts(uri);
  if (!hosts || hosts->next)
    {
      if (hosts)
        msg_error("Multiple hosts found in MongoDB URI",
                  evt_tag_str("uri", proto_str));
      else
        msg_error("No host found in MongoDB URI",
                  evt_tag_str("uri", proto_str));
      g_free(proto_str);
      mongoc_uri_destroy(uri);
      return FALSE;
    }
  *port = hosts->port;
  *host = g_strdup(hosts->host);
  mongoc_uri_destroy(uri);
  if (!*host)
    {
      msg_error("NULL hostname",
                evt_tag_str("uri", proto_str));
      g_free(proto_str);
      return FALSE;
    }
  g_free(proto_str);
  return TRUE;
}

static gboolean
_append_legacy_servers(MongoDBDestDriver *self)
{
  if (self->port != MONGO_CONN_LOCAL)
    {
      if (self->address || self->port)
        {
          gchar *host = NULL;
          gint port = 0;
          if (!_parse_addr(self->address, &host, &port) || (!host))
            {
              msg_error("Cannot parse the primary host",
                        evt_tag_str("primary", self->address),
                        evt_tag_str("driver", self->super.super.super.id));
              return FALSE;
            }
          g_free(host);

          port = self->port ? self->port : MONGOC_DEFAULT_PORT;
          const gchar *address = self->address ? self->address : DEFAULTHOST;
          gchar *srv = g_strdup_printf("%s:%d", address, port);
          self->servers = g_list_prepend(self->servers, srv);
          g_free(self->address);
          self->address = NULL;
          self->port = MONGOC_DEFAULT_PORT;
        }

      if (self->servers)
        {
          GList *l;

          for (l = self->servers; l; l = g_list_next(l))
            {
              gchar *host = NULL;
              gint port = MONGOC_DEFAULT_PORT;

              if (!_parse_addr(l->data, &host, &port))
                {
                  msg_warning("Cannot parse MongoDB server address, ignoring",
                              evt_tag_str("address", l->data),
                              evt_tag_str("driver", self->super.super.super.id));
                  continue;
                }
              host_list_append(&self->recovery_cache, host, port);
              msg_verbose("Added MongoDB server seed",
                          evt_tag_str("host", host),
                          evt_tag_int("port", port),
                          evt_tag_str("driver", self->super.super.super.id));
              g_free(host);
            }
        }
      else
        {
          gchar *localhost = g_strdup_printf(DEFAULTHOST ":%d", MONGOC_DEFAULT_PORT);
          self->servers = g_list_append(NULL, localhost);
          host_list_append(&self->recovery_cache, DEFAULTHOST, MONGOC_DEFAULT_PORT);
        }

      if (!_parse_addr(g_list_nth_data(self->servers, 0), &self->address, &self->port))
        {
          msg_error("Cannot parse the primary host",
                    evt_tag_str("primary", g_list_nth_data(self->servers, 0)),
                    evt_tag_str("driver", self->super.super.super.id));
          return FALSE;
        }
    }
  else
    {
      if (!self->address)
        {
          msg_error("Cannot parse address",
                    evt_tag_str("primary", g_list_nth_data(self->servers, 0)),
                    evt_tag_str("driver", self->super.super.super.id));
          return FALSE;
        }
      host_list_append(&self->recovery_cache, self->address, 0);
    }
  return TRUE;
}

typedef struct _AppendServerState
{
  GString *uri_str;
  gboolean *have_uri;
  gboolean have_path;
} AppendServerState;

static gboolean
_append_server(gpointer user_data, const char *host, gint port)
{
  AppendServerState *state = (AppendServerState *)user_data;
  if (state->have_path || *state->have_uri)
    g_string_append_printf(state->uri_str, ",");
  if (port)
    {
      *state->have_uri = TRUE;
      if (state->have_path)
        {
          msg_warning("Cannot specify both a domain socket and address");
          return FALSE;
        }
      g_string_append_printf(state->uri_str, "%s:%d", host, port);
    }
  else
    {
      state->have_path = TRUE;
      if (*state->have_uri)
        {
          msg_warning("Cannot specify both a domain socket and address");
          return FALSE;
        }
      g_string_append_printf(state->uri_str, "%s", host);
    }
  return TRUE;
}

static gboolean
_append_servers(GString *uri_str, const HostList *host_list, gboolean *have_uri)
{
  *have_uri = FALSE;
  AppendServerState state = {uri_str, have_uri, FALSE};
  return host_list_iterate(host_list, _append_server, &state);
}

static gboolean
_check_auth_options(MongoDBDestDriver *self)
{
  if (self->user || self->password)
    {
      if (!self->user || !self->password)
        {
          msg_error("Neither the username, nor the password can be empty");
          return FALSE;
        }
    }
  return TRUE;
}

gboolean
_build_uri_from_legacy_options(MongoDBDestDriver *self)
{
  if (!_append_legacy_servers(self))
    return FALSE;

  self->uri_str = g_string_new("mongodb://");

  if (self->user && self->password)
    g_string_append_printf(self->uri_str, "%s:%s@", self->user, self->password);

  if (!self->recovery_cache)
    {
      msg_error("Error in host server list",
                evt_tag_str("driver", self->super.super.super.id));
      return FALSE;
    }

  gboolean have_uri;
  if (!_append_servers(self->uri_str, self->recovery_cache, &have_uri))
    return FALSE;

  if (have_uri)
    g_string_append_printf(self->uri_str, "/%s", self->db);

  if (self->safe_mode)
    g_string_append_printf(self->uri_str,
                           "?wtimeoutMS=%d",
                           SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS);
  else
    g_string_append(self->uri_str, "?w=0&safe=false");

  g_string_append_printf(self->uri_str,
                         "&socketTimeoutMS=%d&connectTimeoutMS=%d",
                         SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS,
                         SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS);

  return TRUE;
}

gboolean
afmongodb_dd_create_uri_from_legacy(MongoDBDestDriver *self)
{
  if (!_check_auth_options(self))
    return FALSE;

  if (self->uri_str && self->is_legacy)
    {
      msg_error("Error: either specify a MongoDB URI (and optional collection) or only legacy options",
                evt_tag_str("driver", self->super.super.super.id));
      return FALSE;
    }
  else if (self->is_legacy)
    return _build_uri_from_legacy_options(self);
  else
    return TRUE;
}

void
afmongodb_dd_init_legacy(MongoDBDestDriver *self)
{
  self->db = g_strdup("syslog");
  self->safe_mode = TRUE;
}

void
afmongodb_dd_free_legacy(MongoDBDestDriver *self)
{
  g_free(self->db);
  g_free(self->user);
  g_free(self->password);
  g_free(self->address);
  string_list_free(self->servers);
  host_list_free(self->recovery_cache);
  self->recovery_cache = NULL;
}
