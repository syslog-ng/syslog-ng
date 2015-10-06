/*
 * Copyright (c) 2010-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2014 Gergely Nagy <algernon@balabit.hu>
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


#include "afmongodb.h"
#include "afmongodb-parser.h"
#include "messages.h"
#include "misc.h"
#include "stats/stats-registry.h"
#include "nvtable.h"
#include "logqueue.h"
#include "value-pairs.h"
#include "vptransform.h"
#include "plugin.h"
#include "plugin-types.h"
#include "logthrdestdrv.h"

#include "mongoc.h"
#include <time.h>

#define SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS 60000

typedef struct
{
  gchar *name;
  LogTemplate *value;
} MongoDBField;

typedef struct
{
  LogThrDestDriver super;

  /* Shared between main/writer; only read by the writer, never
     written */
  gchar *db;
  gchar *coll;
  gchar *replica_set;

  GList *servers;
  gchar *address;
  gint port;

  gboolean safe_mode;
  LogTemplateOptions template_options;

  gchar *user;
  gchar *password;

  time_t last_msg_stamp;

  ValuePairs *vp;

  /* Writer-only stuff */
  GList *recovery_cache;
  mongoc_client_t *client;

  gchar *ns;

  GString *current_value;
  bson_t *bson;
} MongoDBDestDriver;

static gboolean
afmongodb_dd_parse_addr (const char *str, char **host, gint *port)
{
  if (!host || !port)
    return FALSE;
  char *protoStr = bson_strdup_printf("mongodb://%s", str);
  mongoc_uri_t *uri = mongoc_uri_new (protoStr);
  free(protoStr);
  if (!uri)
    return FALSE;

  const mongoc_host_list_t *hosts = mongoc_uri_get_hosts (uri);
  if (!hosts || hosts->next)
    {
      mongoc_uri_destroy (uri);
      return FALSE;
    }
  *port = hosts->port;
  *host = g_strdup (hosts->host);
  mongoc_uri_destroy (uri);
  if (!*host)
    return FALSE;
  return TRUE;
}

typedef struct
{
  char *host;
  gint port;
} MongoDBHostPort;

static gboolean
afmongodb_dd_append_host (GList **list, const char *host, gint port)
{
  if (!list)
    return FALSE;
  MongoDBHostPort *hp = g_new0(MongoDBHostPort, 1);
  hp->host = g_strdup(host);
  hp->port = port;
  *list = g_list_prepend(*list, hp);
  return TRUE;
}

/*
 * Configuration
 */

void
afmongodb_dd_set_user(LogDriver *d, const gchar *user)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->user);
  self->user = g_strdup(user);
}

void
afmongodb_dd_set_password(LogDriver *d, const gchar *password)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->password);
  self->password = g_strdup(password);
}

void
afmongodb_dd_set_host(LogDriver *d, const gchar *host)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once("WARNING: Using host() option is deprecated in mongodb driver, please use servers() instead", NULL);

  g_free(self->address);
  self->address = g_strdup(host);
}

void
afmongodb_dd_set_port(LogDriver *d, gint port)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  msg_warning_once("WARNING: Using port() option is deprecated in mongodb driver, please use servers() instead", NULL);

  self->port = port;
}

void
afmongodb_dd_set_servers(LogDriver *d, GList *servers)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  string_list_free(self->servers);
  self->servers = servers;
}

void
afmongodb_dd_set_path(LogDriver *d, const gchar *path)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->address);
  self->address = g_strdup(path);
  self->port = MONGOC_DEFAULT_PORT;
}

void
afmongodb_dd_set_replica_set(LogDriver *d, const gchar *replica_set)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->replica_set);
  self->replica_set = g_strdup(replica_set);
}

LogTemplateOptions *
afmongodb_dd_get_template_options(LogDriver *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *) s;

  return &self->template_options;
}

gboolean
afmongodb_dd_check_address(LogDriver *d, gboolean local)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if (local)
    {
      if ((self->port != 0 ||
           self->port != MONGOC_DEFAULT_PORT) &&
          self->address != NULL)
        return FALSE;
      if (self->servers)
        return FALSE;
    }
  else
    {
      if (self->port == MONGOC_DEFAULT_PORT &&
          self->address != NULL)
        return FALSE;
    }
  return TRUE;
}

void
afmongodb_dd_set_database(LogDriver *d, const gchar *database)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->db);
  self->db = g_strdup(database);
}

void
afmongodb_dd_set_collection(LogDriver *d, const gchar *collection)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->coll);
  self->coll = g_strdup(collection);
}

void
afmongodb_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if (self->vp)
    value_pairs_unref (self->vp);
  self->vp = vp;
}

void
afmongodb_dd_set_safe_mode(LogDriver *d, gboolean state)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->safe_mode = state;
}

/*
 * Utilities
 */

static gchar *
afmongodb_dd_format_stats_instance(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;
  static gchar persist_name[1024];

  if (self->port == MONGOC_DEFAULT_PORT)
    g_snprintf(persist_name, sizeof(persist_name),
               "mongodb,%s,%s,%s", self->address, self->db, self->coll);
  else
    g_snprintf(persist_name, sizeof(persist_name),
               "mongodb,%s,%u,%s,%s", self->address, self->port, self->db, self->coll);
  return persist_name;
}

static gchar *
afmongodb_dd_format_persist_name(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;
  static gchar persist_name[1024];

  if (self->port == MONGOC_DEFAULT_PORT)
    g_snprintf(persist_name, sizeof(persist_name),
               "afmongodb(%s,%s,%s)", self->address, self->db, self->coll);
  else
    g_snprintf(persist_name, sizeof(persist_name),
               "afmongodb(%s,%u,%s,%s)", self->address, self->port, self->db, self->coll);
  return persist_name;
}

static void
afmongodb_dd_disconnect(LogThrDestDriver *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  mongoc_client_destroy(self->client);
  self->client = NULL;
}

static gboolean
afmongodb_dd_connect(MongoDBDestDriver *self, gboolean reconnect)
{
  bson_string_t *uri_str;

  if (reconnect && self->client)
    return TRUE;

  uri_str = bson_string_new ("mongodb://");
  if (NULL == uri_str)
    return FALSE;

  if (self->user || self->password)
    {
      bson_string_append_printf (uri_str, "%s:%s", self->user, self->password);
    }

  GList *iterator = self->recovery_cache;
  if (!iterator)
    {
      msg_error ("Error in host server list", evt_tag_str ("driver", self->super.super.super.id), NULL);
      return FALSE;
    }
  MongoDBHostPort *hp = iterator->data;
  bson_string_append_printf (uri_str, "%s:%hu", hp->host, hp->port);
  while (iterator->next)
    {
      iterator = iterator->next;
      hp = iterator->data;
      bson_string_append_printf (uri_str, ",%s:%hu", hp->host, hp->port);
    }

  bson_string_append_printf (uri_str, "/%s", self->db);

  bson_string_append_printf (uri_str, "?slaveOk=true&sockettimeoutms=%d",
                             SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS);

  if (self->replica_set)
    bson_string_append_printf (uri_str, "&replicaSet=%s", self->replica_set);

  msg_debug(uri_str->str, evt_tag_str ("driver", self->super.super.super.id),
            NULL);

  self->client = mongoc_client_new (uri_str->str);
  bson_string_free (uri_str, TRUE);

  if (!self->client)
    {
      msg_error("Error connecting to MongoDB",
                evt_tag_str ("driver", self->super.super.super.id), NULL);
      return FALSE;
    }

  return TRUE;
}

/*
 * Worker thread
 */
static gboolean
afmongodb_vp_obj_start(const gchar *name,
                       const gchar *prefix, gpointer *prefix_data,
                       const gchar *prev, gpointer *prev_data,
                       gpointer user_data)
{
  bson_t *o;

  if (prefix_data)
    {
      o = bson_new();
      *prefix_data = o;
    }
  return FALSE;
}

static gboolean
afmongodb_vp_obj_end(const gchar *name,
                     const gchar *prefix, gpointer *prefix_data,
                     const gchar *prev, gpointer *prev_data,
                     gpointer user_data)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)user_data;
  bson_t *root;

  if (prev_data)
    root = (bson_t *)*prev_data;
  else
    root = self->bson;

  if (prefix_data)
    {
      bson_t *d = (bson_t *)*prefix_data;

      bson_append_document (root, name, -1, d);
      bson_free (d);
    }
  return FALSE;
}

static gboolean
afmongodb_vp_process_value(const gchar *name, const gchar *prefix,
                           TypeHint type, const gchar *value,
                           gpointer *prefix_data, gpointer user_data)
{
  bson_t *o;
  MongoDBDestDriver *self = (MongoDBDestDriver *)user_data;
  gboolean fallback = self->template_options.on_error & ON_ERROR_FALLBACK_TO_STRING;

  if (prefix_data)
    o = (bson_t *)*prefix_data;
  else
    o = self->bson;

  switch (type)
    {
    case TYPE_HINT_BOOLEAN:
      {
        gboolean b;

        if (type_cast_to_boolean (value, &b, NULL))
          bson_append_bool (o, name, -1, b);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "boolean");

            if (fallback)
              bson_append_utf8 (o, name, -1, value, -1);
            else
              return r;
          }
        break;
      }
    case TYPE_HINT_INT32:
      {
        gint32 i;

        if (type_cast_to_int32 (value, &i, NULL))
          bson_append_int32 (o, name, -1, i);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "int32");

            if (fallback)
              bson_append_utf8 (o, name, -1, value, -1);
            else
              return r;
          }
        break;
      }
    case TYPE_HINT_INT64:
      {
        gint64 i;

        if (type_cast_to_int64 (value, &i, NULL))
          bson_append_int64 (o, name, -1, i);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "int64");

            if (fallback)
              bson_append_utf8(o, name, -1, value, -1);
            else
              return r;
          }

        break;
      }
    case TYPE_HINT_DOUBLE:
      {
        gdouble d;

        if (type_cast_to_double (value, &d, NULL))
          bson_append_double (o, name, -1, d);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "double");
            if (fallback)
              bson_append_utf8(o, name, -1, value, -1);
            else
              return r;
          }

        break;
      }
    case TYPE_HINT_DATETIME:
      {
        guint64 i;

        if (type_cast_to_datetime_int (value, &i, NULL))
          bson_append_date_time (o, name, -1, (gint64)i);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "datetime");

            if (fallback)
              bson_append_utf8(o, name, -1, value, -1);
            else
              return r;
          }

        break;
      }
    case TYPE_HINT_STRING:
    case TYPE_HINT_LITERAL:
      bson_append_utf8 (o, name, -1, value, -1);
      break;
    default:
      return TRUE;
    }

  return FALSE;
}

static void
afmongodb_worker_retry_over_message(LogThrDestDriver *s, LogMessage *msg)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  msg_error("Multiple failures while inserting this record into the database, "
            "message dropped",
            evt_tag_str("driver", self->super.super.super.id),
            evt_tag_int("number_of_retries", s->retries.max),
            evt_tag_value_pairs("message", self->vp, msg,
                                self->super.seq_num,
                                LTZ_SEND, &self->template_options),
            NULL);
}

static worker_insert_result_t
afmongodb_worker_insert (LogThrDestDriver *s, LogMessage *msg)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  gboolean success;
  gboolean drop_silently = self->template_options.on_error & ON_ERROR_SILENT;

  if (!afmongodb_dd_connect(self, TRUE))
    return WORKER_INSERT_RESULT_NOT_CONNECTED;

  bson_reinit (self->bson);

  success = value_pairs_walk(self->vp,
                             afmongodb_vp_obj_start,
                             afmongodb_vp_process_value,
                             afmongodb_vp_obj_end,
                             msg, self->super.seq_num,
                             LTZ_SEND,
                             &self->template_options,
                             self);

  if (!success)
    {
      if (!drop_silently)
        {
          msg_error("Failed to format message for MongoDB, dropping message",
                    evt_tag_value_pairs("message", self->vp, msg,
                                        self->super.seq_num,
                                        LTZ_SEND, &self->template_options),
                    evt_tag_str("driver", self->super.super.super.id),
                    NULL);
        }
      return WORKER_INSERT_RESULT_DROP;
    }
  else
    {
      bson_error_t error;
      msg_debug("Outgoing message to MongoDB destination",
                evt_tag_value_pairs("message", self->vp, msg,
                                    self->super.seq_num,
                                    LTZ_SEND, &self->template_options),
                evt_tag_str("driver", self->super.super.super.id),
                NULL);
      mongoc_collection_t *collection = mongoc_client_get_collection (
          self->client, self->db, self->coll);
      success = NULL != collection;
      if (success)
        {
          success = mongoc_collection_insert (collection, MONGOC_INSERT_NONE,
                                              (const bson_t *) self->bson,
                                              NULL,
                                              &error);
          mongoc_collection_destroy (collection);
        }

      if (!success)
        {
          msg_error("Network error while inserting into MongoDB",
                    evt_tag_int("time_reopen", self->super.time_reopen),
                    evt_tag_str("reason", error.message),
                    evt_tag_str("driver", self->super.super.super.id),
                    NULL);
        }
    }

  if (!success && (errno == ENOTCONN))
    return WORKER_INSERT_RESULT_NOT_CONNECTED;

  if (!success)
    return WORKER_INSERT_RESULT_ERROR;

  return WORKER_INSERT_RESULT_SUCCESS;
}

static void
afmongodb_worker_thread_init(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  afmongodb_dd_connect(self, FALSE);

  self->ns = g_strconcat (self->db, ".", self->coll, NULL);

  self->current_value = g_string_sized_new(256);

  self->bson = bson_sized_new(4096);
}

static void
afmongodb_worker_thread_deinit(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free (self->ns);
  g_string_free (self->current_value, TRUE);

  bson_free (self->bson);
}

/*
 * Main thread
 */

static gboolean
afmongodb_dd_check_auth_options(MongoDBDestDriver *self)
{
  if (self->user || self->password)
    {
      if (!self->user || !self->password)
        {
          msg_error("Neither the username, nor the password can be empty", NULL);
          return FALSE;
        }
    }
  return TRUE;
}

static void
afmongodb_dd_init_value_pairs_dot_to_underscore_transformation(MongoDBDestDriver *self)
{
  ValuePairsTransformSet *vpts;

 /* Always replace a leading dot with an underscore. */
  vpts = value_pairs_transform_set_new(".*");
  value_pairs_transform_set_add_func(vpts, value_pairs_new_transform_replace_prefix(".", "_"));
  value_pairs_add_transforms(self->vp, vpts);
};

static gboolean
afmongodb_dd_init(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  if (!afmongodb_dd_check_auth_options(self))
    return FALSE;

  afmongodb_dd_init_value_pairs_dot_to_underscore_transformation(self);
  if (self->port != MONGOC_DEFAULT_PORT)
    {
      if (self->address)
        {
          gchar *srv = g_strdup_printf ("%s:%d", self->address,
                                        (self->port) ? self->port : MONGOC_DEFAULT_PORT);
          self->servers = g_list_prepend (self->servers, srv);
          g_free (self->address);
        }

      if (self->servers)
        {
          GList *l;

          for (l=self->servers; l; l = g_list_next(l))
            {
              gchar *host = NULL;
              gint port = MONGOC_DEFAULT_PORT;

              if (!afmongodb_dd_parse_addr(l->data, &host, &port))
                {
                  msg_warning("Cannot parse MongoDB server address, ignoring",
                              evt_tag_str("address", l->data),
                              evt_tag_str("driver", self->super.super.super.id),
                              NULL);
                  continue;
                }
              afmongodb_dd_append_host (&self->recovery_cache, host, port);
              msg_verbose("Added MongoDB server seed",
                          evt_tag_str("host", host),
                          evt_tag_int("port", port),
                          evt_tag_str("driver", self->super.super.super.id),
                          NULL);
              g_free(host);
            }
        }
      else
        {
          gchar *port = g_strdup_printf ("%d", MONGOC_DEFAULT_PORT);
          gchar *localhost = g_strconcat ("127.0.0.1", port, NULL);
          g_free(port);
          afmongodb_dd_set_servers((LogDriver *)self, g_list_append (NULL, localhost));
          afmongodb_dd_append_host (&self->recovery_cache, "127.0.0.1", MONGOC_DEFAULT_PORT);
        }

      self->address = NULL;
      self->port = MONGOC_DEFAULT_PORT;
      if (!afmongodb_dd_parse_addr(g_list_nth_data(self->servers, 0),
                                 &self->address,
                                 &self->port))
        {
          msg_error("Cannot parse the primary host",
                    evt_tag_str("primary", g_list_nth_data(self->servers, 0)),
                    evt_tag_str("driver", self->super.super.super.id),
                    NULL);
          return FALSE;
        }
    }
  else
    {
      if (!self->address)
        {
          msg_error("Cannot parse address",
                    evt_tag_str ("primary", g_list_nth_data (self->servers, 0)),
                    evt_tag_str ("driver", self->super.super.super.id), NULL);
          return FALSE;
        }
      afmongodb_dd_append_host (&self->recovery_cache, self->address,
                                self->port);
    }

  if (self->port == MONGOC_DEFAULT_PORT)
    msg_verbose("Initializing MongoDB destination",
                evt_tag_str("address", self->address),
                evt_tag_str("database", self->db),
                evt_tag_str("collection", self->coll),
                evt_tag_str("driver", self->super.super.super.id),
                NULL);
  else
    msg_verbose("Initializing MongoDB destination",
                evt_tag_str("address", self->address),
                evt_tag_int("port", self->port),
                evt_tag_str("database", self->db),
                evt_tag_str("collection", self->coll),
                evt_tag_str("driver", self->super.super.super.id),
                NULL);

  return log_threaded_dest_driver_start(s);
}

static void
afmongodb_dd_free_host_list (GList **list)
{
  GList *it = *list;
  while (it)
    {
      MongoDBHostPort *hp = it->data;
      g_free(hp->host);
      it = it->next;
    }
  g_list_foreach(*list, (GFunc)g_free, NULL);
  g_list_free (*list);
  *list = NULL;
}

static void
afmongodb_dd_free(LogPipe *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  g_free(self->db);
  g_free(self->coll);
  g_free(self->replica_set);
  g_free(self->user);
  g_free(self->password);
  g_free(self->address);
  string_list_free(self->servers);
  if (self->vp)
    value_pairs_unref(self->vp);

  afmongodb_dd_free_host_list(&self->recovery_cache);
  mongoc_cleanup ();
  log_threaded_dest_driver_free(d);
}

static void
afmongodb_dd_queue_method(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->last_msg_stamp = cached_g_current_time_sec ();
}

/*
 * Plugin glue.
 */

LogDriver *
afmongodb_dd_new(GlobalConfig *cfg)
{
  MongoDBDestDriver *self = g_new0(MongoDBDestDriver, 1);

  mongoc_init ();

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = afmongodb_dd_init;
  self->super.super.super.super.free_fn = afmongodb_dd_free;
  self->super.queue_method = afmongodb_dd_queue_method;

  self->super.worker.thread_init = afmongodb_worker_thread_init;
  self->super.worker.thread_deinit = afmongodb_worker_thread_deinit;
  self->super.worker.disconnect = afmongodb_dd_disconnect;
  self->super.worker.insert = afmongodb_worker_insert;
  self->super.format.stats_instance = afmongodb_dd_format_stats_instance;
  self->super.format.persist_name = afmongodb_dd_format_persist_name;
  self->super.stats_source = SCS_MONGODB;
  self->super.messages.retry_over = afmongodb_worker_retry_over_message;

  afmongodb_dd_set_database((LogDriver *)self, "syslog");
  afmongodb_dd_set_collection((LogDriver *)self, "messages");
  afmongodb_dd_set_safe_mode((LogDriver *)self, TRUE);

  self->replica_set = NULL;
  log_template_options_defaults(&self->template_options);
  afmongodb_dd_set_value_pairs(&self->super.super.super, value_pairs_new_default(cfg));

  self->recovery_cache = NULL;

  return (LogDriver *)self;
}

extern CfgParser afmongodb_dd_parser;

static Plugin afmongodb_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "mongodb",
  .parser = &afmongodb_parser,
};

gboolean
afmongodb_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &afmongodb_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "afmongodb",
  .version = VERSION,
  .description = "The afmongodb module provides MongoDB destination support for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = &afmongodb_plugin,
  .plugins_len = 1,
};
