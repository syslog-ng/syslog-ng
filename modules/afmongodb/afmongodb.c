/*
 * Copyright (c) 2010-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2013 Gergely Nagy <algernon@balabit.hu>
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

#include "mongo.h"
#include <time.h>

#define MAX_RETRIES_OF_FAILED_INSERT_DEFAULT 3
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

  GList *servers;
  gchar *address;
  gint port;

  gboolean safe_mode;
  LogTemplateOptions template_options;

  gchar *user;
  gchar *password;
  gint failed_message_counter;
  gint max_retry_of_failed_inserts;

  time_t last_msg_stamp;

  ValuePairs *vp;

  /* Writer-only stuff */
  mongo_sync_conn_recovery_cache *recovery_cache;
  mongo_sync_connection *conn;
  gint32 seq_num;

  gchar *ns;

  GString *current_value;
  bson *bson;
} MongoDBDestDriver;

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

  static gboolean host_warning_displayed = FALSE;
  if (!host_warning_displayed)
    {
      msg_warning("WARNING! Using host() option is deprecated in mongodb driver, please use servers() instead!", NULL);
      host_warning_displayed = TRUE;
    }

  g_free(self->address);
  self->address = g_strdup(host);
}

void
afmongodb_dd_set_port(LogDriver *d, gint port)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  static gboolean port_warning_displayed = FALSE;
  if (!port_warning_displayed)
    {
      msg_warning("WARNING! Using port() option is deprecated in mongodb driver, please use servers() instead!", NULL);
      port_warning_displayed = TRUE;
    }

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
  self->port = MONGO_CONN_LOCAL;
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
           self->port != MONGO_CONN_LOCAL) &&
          self->address != NULL)
        return FALSE;
      if (self->servers)
        return FALSE;
    }
  else
    {
      if (self->port == MONGO_CONN_LOCAL &&
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
    value_pairs_free (self->vp);
  self->vp = vp;
}

void
afmongodb_dd_set_safe_mode(LogDriver *d, gboolean state)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->safe_mode = state;
}

void
afmongodb_dd_set_retries(LogDriver *d, gint retries)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->max_retry_of_failed_inserts = retries;
};
/*
 * Utilities
 */

static gchar *
afmongodb_dd_format_stats_instance(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;
  static gchar persist_name[1024];

  if (self->port == MONGO_CONN_LOCAL)
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

  if (self->port == MONGO_CONN_LOCAL)
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

  mongo_sync_disconnect(self->conn);
  self->conn = NULL;
}

static gboolean
afmongodb_dd_connect(MongoDBDestDriver *self, gboolean reconnect)
{
  if (reconnect && self->conn)
    return TRUE;

  self->conn = mongo_sync_connect_recovery_cache(self->recovery_cache, FALSE);

  if (!self->conn)
    {
      msg_error ("Error connecting to MongoDB", NULL);
      return FALSE;
    }

  mongo_connection_set_timeout((mongo_connection*) self->conn, SOCKET_TIMEOUT_FOR_MONGO_CONNECTION_IN_MILLISECS);

  mongo_sync_conn_set_safe_mode(self->conn, self->safe_mode);


  if (self->user || self->password)
    {
      if (!mongo_sync_cmd_authenticate (self->conn, self->db,
                                        self->user, self->password))
        {
          msg_error("MongoDB authentication failed", NULL);
          return FALSE;
        }
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
  bson *o;

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
  bson *root;

  if (prev_data)
    root = (bson *)*prev_data;
  else
    root = self->bson;

  if (prefix_data)
    {
      bson *d = (bson *)*prefix_data;

      bson_finish (d);
      bson_append_document (root, name, d);
      bson_free (d);
    }
  return FALSE;
}

static gboolean
afmongodb_vp_process_value(const gchar *name, const gchar *prefix,
                           TypeHint type, const gchar *value,
                           gpointer *prefix_data, gpointer user_data)
{
  bson *o;
  MongoDBDestDriver *self = (MongoDBDestDriver *)user_data;
  gboolean fallback = self->template_options.on_error & ON_ERROR_FALLBACK_TO_STRING;

  if (prefix_data)
    o = (bson *)*prefix_data;
  else
    o = self->bson;

  switch (type)
    {
    case TYPE_HINT_BOOLEAN:
      {
        gboolean b;

        if (type_cast_to_boolean (value, &b, NULL))
          bson_append_boolean (o, name, b);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "boolean");

            if (fallback)
              bson_append_string (o, name, value, -1);
            else
              return r;
          }
        break;
      }
    case TYPE_HINT_INT32:
      {
        gint32 i;

        if (type_cast_to_int32 (value, &i, NULL))
          bson_append_int32 (o, name, i);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "int32");

            if (fallback)
              bson_append_string (o, name, value, -1);
            else
              return r;
          }
        break;
      }
    case TYPE_HINT_INT64:
      {
        gint64 i;

        if (type_cast_to_int64 (value, &i, NULL))
          bson_append_int64 (o, name, i);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "int64");

            if (fallback)
              bson_append_string(o, name, value, -1);
            else
              return r;
          }

        break;
      }
    case TYPE_HINT_DATETIME:
      {
        guint64 i;

        if (type_cast_to_datetime_int (value, &i, NULL))
          bson_append_utc_datetime (o, name, (gint64)i);
        else
          {
            gboolean r = type_cast_drop_helper(self->template_options.on_error,
                                               value, "datetime");

            if (fallback)
              bson_append_string(o, name, value, -1);
            else
              return r;
          }

        break;
      }
    case TYPE_HINT_STRING:
    case TYPE_HINT_LITERAL:
      bson_append_string (o, name, value, -1);
      break;
    default:
      return TRUE;
    }

  return FALSE;
}

static void
afmongodb_worker_drop_message(MongoDBDestDriver *self, LogMessage *msg, LogPathOptions *path_options)
{
  stats_counter_inc(self->super.dropped_messages);
  step_sequence_number(&self->seq_num);
  log_msg_drop(msg, path_options);
  self->failed_message_counter = 0;
};

static void
afmongodb_worker_accept_message(MongoDBDestDriver *self, LogMessage *msg, LogPathOptions *path_options)
{
  step_sequence_number(&self->seq_num);
  log_msg_ack(msg, path_options);
  log_msg_unref(msg);
  self->failed_message_counter = 0;
}

static gboolean
afmongodb_worker_insert (LogThrDestDriver *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  gboolean success;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  if (!afmongodb_dd_connect(self, TRUE))
    return FALSE;

  success = log_queue_pop_head(self->super.queue, &msg, &path_options, FALSE, FALSE);
  if (!success)
    return TRUE;

  msg_set_context(msg);

  bson_reset (self->bson);

  success = value_pairs_walk(self->vp,
                             afmongodb_vp_obj_start,
                             afmongodb_vp_process_value,
                             afmongodb_vp_obj_end,
                             msg, self->seq_num, LTZ_SEND,
                             &self->template_options,
                             self);
  bson_finish (self->bson);

  if (!success)
    {
      msg_error("Failed to format message for MongoDB, dropping message",
                 evt_tag_value_pairs("message", self->vp, msg, self->seq_num, LTZ_SEND, &self->template_options),
                 NULL);
      msg_set_context(NULL);
      afmongodb_worker_drop_message(self, msg, &path_options);
      return TRUE;
    }
  else
    {
      msg_debug("Outgoing message to MongoDB destination",
                 evt_tag_value_pairs("message", self->vp, msg, self->seq_num, LTZ_SEND, &self->template_options),
                 NULL);
      if (!mongo_sync_cmd_insert_n(self->conn, self->ns, 1,
                                   (const bson **)&self->bson))
        {
          msg_error("Network error while inserting into MongoDB",
                    evt_tag_int("time_reopen", self->super.time_reopen),
                    NULL);
          success = FALSE;
        }
    }

  msg_set_context(NULL);

  if (success)
    {
      afmongodb_worker_accept_message(self, msg, &path_options);
    }
  else
    {
      self->failed_message_counter++;
      if (self->failed_message_counter >= self->max_retry_of_failed_inserts)
        {
          msg_error("Multiple failures while inserting this record into the database, message dropped",
                    evt_tag_int("number_of_retries", self->max_retry_of_failed_inserts),
                    evt_tag_value_pairs("message", self->vp, msg, self->seq_num, LTZ_SEND, &self->template_options),
                    NULL);
          afmongodb_worker_drop_message(self, msg, &path_options);
        }
      else
        log_queue_push_head(self->super.queue, msg, &path_options);
    }

  return success;
}

static void
afmongodb_worker_thread_init(LogThrDestDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  afmongodb_dd_connect(self, FALSE);

  self->ns = g_strconcat (self->db, ".", self->coll, NULL);

  self->current_value = g_string_sized_new(256);

  self->bson = bson_new_sized(4096);
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

  if (self->max_retry_of_failed_inserts <= 0)
    {
      msg_warning("WARNING! Wrong value for retries in MongoDB destination, setting it to default",
                   evt_tag_int("default", MAX_RETRIES_OF_FAILED_INSERT_DEFAULT),
                   NULL);

      self->max_retry_of_failed_inserts = MAX_RETRIES_OF_FAILED_INSERT_DEFAULT;
    }

  if (self->super.super.throttle != 0)
    {
      msg_warning("WARNING! Throttle option for MongoDB is not supported, ignoring!", NULL);
      self->super.super.throttle = 0;
    }

  if (!afmongodb_dd_check_auth_options(self))
    return FALSE;

  afmongodb_dd_init_value_pairs_dot_to_underscore_transformation(self);
  if (self->port != MONGO_CONN_LOCAL)
    {
      if (self->address)
        {
          gchar *srv = g_strdup_printf ("%s:%d", self->address,
                                        (self->port) ? self->port : 27017);
          self->servers = g_list_prepend (self->servers, srv);
          g_free (self->address);
        }

      if (self->servers)
        {
          GList *l;

          l = self->servers;
          while ((l = g_list_next(l)) != NULL)
            {
              gchar *host = NULL;
              gint port = 27017;

              if (!mongo_util_parse_addr(l->data, &host, &port))
                {
                  msg_warning("Cannot parse MongoDB server address, ignoring",
                              evt_tag_str("address", l->data),
                              NULL);
                  continue;
                }
              mongo_sync_conn_recovery_cache_seed_add (self->recovery_cache, host, port);
              msg_verbose("Added MongoDB server seed",
                          evt_tag_str("host", host),
                          evt_tag_int("port", port),
                          NULL);
              g_free(host);
            }
        }
      else
        {
          afmongodb_dd_set_servers((LogDriver *)self, g_list_append (NULL, g_strdup ("127.0.0.1:27017")));
          mongo_sync_conn_recovery_cache_seed_add (self->recovery_cache, "127.0.0.1", 27017);
        }

      self->address = NULL;
      self->port = 27017;
      if (!mongo_util_parse_addr(g_list_nth_data(self->servers, 0),
                                 &self->address,
                                 &self->port))
        {
          msg_error("Cannot parse the primary host",
                    evt_tag_str("primary", g_list_nth_data(self->servers, 0)),
                    NULL);
          return FALSE;
        }
    }

  if (self->port == MONGO_CONN_LOCAL)
    msg_verbose("Initializing MongoDB destination",
                evt_tag_str("address", self->address),
                evt_tag_str("database", self->db),
                evt_tag_str("collection", self->coll),
                NULL);
  else
    msg_verbose("Initializing MongoDB destination",
                evt_tag_str("address", self->address),
                evt_tag_int("port", self->port),
                evt_tag_str("database", self->db),
                evt_tag_str("collection", self->coll),
                NULL);

  return log_threaded_dest_driver_start(s);
}

static void
afmongodb_dd_free(LogPipe *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  g_free(self->db);
  g_free(self->coll);
  g_free(self->user);
  g_free(self->password);
  g_free(self->address);
  string_list_free(self->servers);
  if (self->vp)
    value_pairs_free(self->vp);

  mongo_sync_conn_recovery_cache_free(self->recovery_cache);
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

  mongo_util_oid_init (0);

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

  afmongodb_dd_set_database((LogDriver *)self, "syslog");
  afmongodb_dd_set_collection((LogDriver *)self, "messages");
  afmongodb_dd_set_safe_mode((LogDriver *)self, FALSE);

  init_sequence_number(&self->seq_num);


  self->max_retry_of_failed_inserts = MAX_RETRIES_OF_FAILED_INSERT_DEFAULT;
  self->safe_mode = TRUE;

  log_template_options_defaults(&self->template_options);
  afmongodb_dd_set_value_pairs(&self->super.super.super, value_pairs_new_default(cfg));

  self->recovery_cache = mongo_sync_conn_recovery_cache_new();

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
