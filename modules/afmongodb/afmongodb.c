/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2011 Gergely Nagy <algernon@balabit.hu>
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

#include <time.h>

#include "afmongodb.h"
#include "afmongodb-parser.h"
#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats.h"
#include "nvtable.h"
#include "logqueue.h"

#include "mongo.h"

typedef struct
{
  gchar *name;
  LogTemplate *value;
} MongoDBField;

typedef struct
{
  LogDriver super;

  /* Shared between main/writer; only read by the writer, never
     written */
  gchar *db;
  LogTemplate *coll;

  gchar *host;
  gint port;

  gchar *user;
  gchar *password;

  GList *keys;
  GList *values;

  gint num_fields;
  MongoDBField *fields;

  time_t time_reopen;

  guint32 *dropped_messages;
  guint32 *stored_messages;

  guint32 log_fifo_size;

  time_t last_msg_stamp;

  /* Thread related stuff; shared */
  GThread *writer_thread;
  GMutex *queue_mutex;
  GMutex *suspend_mutex;
  GCond *writer_thread_wakeup_cond;

  gboolean writer_thread_terminate;
  gboolean writer_thread_suspended;
  GTimeVal writer_thread_suspend_target;

  LogQueue *queue;

  /* Writer-only stuff */
  mongo_connection *conn;
  gint32 seq_num;

  GString *current_namespace;
  gint ns_prefix_len;

  GString *current_value;

  bson *bson_sel, *bson_upd, *bson_set;
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

  g_free(self->host);
  self->host = g_strdup (host);
}

void
afmongodb_dd_set_port(LogDriver *d, gint port)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->port = (int)port;
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

  log_template_unref(self->coll);
  self->coll = log_template_new(log_pipe_get_config(&d->super), NULL, collection);
}

void
afmongodb_dd_set_keys(LogDriver *d, GList *keys)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  string_list_free(self->keys);
  self->keys = keys;
}

void
afmongodb_dd_set_values(LogDriver *d, GList *values)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  string_list_free(self->values);
  self->values = values;
}

void
afmongodb_dd_set_log_fifo_size(LogDriver *d, guint32 size)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->log_fifo_size = size;
}

/*
 * Utilities
 */

static gchar *
afmongodb_dd_format_stats_instance(MongoDBDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
	     "mongodb,%s,%u,%s", self->host, self->port, self->db);
  return persist_name;
}

static void
afmongodb_dd_suspend(MongoDBDestDriver *self)
{
  self->writer_thread_suspended = TRUE;
  g_get_current_time(&self->writer_thread_suspend_target);
  g_time_val_add(&self->writer_thread_suspend_target,
		 self->time_reopen * 1000000);
}

static void
afmongodb_dd_disconnect(MongoDBDestDriver *self)
{
  mongo_disconnect(self->conn);
  self->conn = NULL;
}

static gboolean
afmongodb_dd_connect(MongoDBDestDriver *self, gboolean reconnect)
{
  if (reconnect && self->conn)
    return TRUE;

  self->conn = mongo_connect(self->host, self->port);

  if (!self->conn)
    {
      msg_error ("Error connecting to MongoDB", NULL);
      return FALSE;
    }

  /*
  if (self->user || self->password)
    {
      if (!self->user || !self->password)
	{
	  msg_error("Neither the username, nor the password can be empty", NULL);
	  return FALSE;
	}

      if (mongo_cmd_authenticate(&self->mongo_conn, self->db, self->user, self->password) != 1)
	{
	  msg_error("MongoDB authentication failed", NULL);
	  return FALSE;
	}
    }
  */

  return TRUE;
}

/*
 * Worker thread
 */

static gboolean
afmongodb_worker_insert (MongoDBDestDriver *self)
{
  gint i;
  gboolean success;
  mongo_packet *p;
  guint8 *oid;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  afmongodb_dd_connect(self, TRUE);

  g_mutex_lock(self->queue_mutex);
  success = log_queue_pop_head(self->queue, &msg, &path_options, 0);
  g_mutex_unlock(self->queue_mutex);
  if (!success)
    return TRUE;

  msg_set_context(msg);

  bson_reset (self->bson_sel);
  bson_reset (self->bson_upd);
  bson_reset (self->bson_set);

  oid = mongo_util_oid_new_with_time (self->last_msg_stamp, self->seq_num);
  bson_append_oid (self->bson_sel, "_id", oid);
  g_free (oid);
  bson_finish (self->bson_sel);

  g_string_truncate(self->current_namespace, self->ns_prefix_len);
  log_template_append_format(self->coll, msg, NULL, LTZ_LOCAL,
			     self->seq_num, NULL, self->current_namespace);

  for (i = 0; i < self->num_fields; i++)
    {
      log_template_format(self->fields[i].value, msg, NULL, LTZ_SEND,
			  self->seq_num, NULL, self->current_value);
      if (self->current_value->len)
	bson_append_string(self->bson_set, self->fields[i].name,
			   self->current_value->str, -1);
    }

  bson_finish (self->bson_set);

  bson_append_document (self->bson_upd, "$set", self->bson_set);
  bson_finish (self->bson_upd);

  p = mongo_wire_cmd_update (1, self->current_namespace->str, 1,
			     self->bson_sel, self->bson_upd);

  if (!mongo_packet_send (self->conn, p))
    {
      msg_error ("Network error while inserting into MongoDB",
		 evt_tag_int("time_reopen", self->time_reopen),
		 NULL);
      success = FALSE;
    }

  mongo_wire_packet_free (p);

  msg_set_context(NULL);

  if (success)
    {
      stats_counter_inc(self->stored_messages);
      step_sequence_number(&self->seq_num);
      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }
  else
    {
      g_mutex_lock(self->queue_mutex);
      log_queue_push_head(self->queue, msg, &path_options);
      g_mutex_unlock(self->queue_mutex);
    }

  return success;
}

static gpointer
afmongodb_worker_thread (gpointer arg)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)arg;
  gboolean success;

  msg_debug ("Worker thread started",
	     evt_tag_str("driver", self->super.id),
	     NULL);

  success = afmongodb_dd_connect(self, FALSE);
  self->current_namespace = g_string_sized_new(64);
  self->ns_prefix_len = strlen (self->db) + 1;

  self->current_namespace =
    g_string_append_c (g_string_assign (self->current_namespace,
					self->db), '.');

  self->current_value = g_string_sized_new(256);

  self->bson_sel = bson_new_sized(64);
  self->bson_upd = bson_new_sized(512);
  self->bson_set = bson_new_sized(512);

  while (!self->writer_thread_terminate)
    {
      g_mutex_lock(self->suspend_mutex);
      if (self->writer_thread_suspended)
	{
	  g_cond_timed_wait(self->writer_thread_wakeup_cond,
			    self->suspend_mutex,
			    &self->writer_thread_suspend_target);
	  self->writer_thread_suspended = FALSE;
	  g_mutex_unlock(self->suspend_mutex);
	}
      else
	{
	  g_mutex_unlock(self->suspend_mutex);

	  g_mutex_lock(self->queue_mutex);
	  if (log_queue_get_length(self->queue) == 0)
	    {
	      g_cond_wait(self->writer_thread_wakeup_cond, self->queue_mutex);
	    }
	  g_mutex_unlock(self->queue_mutex);
	}

      if (self->writer_thread_terminate)
	break;

      if (!afmongodb_worker_insert (self))
	{
	  afmongodb_dd_disconnect(self);
	  afmongodb_dd_suspend(self);
	}
    }

  afmongodb_dd_disconnect(self);

  g_string_free (self->current_namespace, TRUE);
  g_string_free (self->current_value, TRUE);

  bson_free (self->bson_sel);
  bson_free (self->bson_upd);
  bson_free (self->bson_set);

  msg_debug ("Worker thread finished",
	     evt_tag_str("driver", self->super.id),
	     NULL);

  return NULL;
}

/*
 * Main thread
 */

static void
afmongodb_dd_start_thread (MongoDBDestDriver *self)
{
  self->writer_thread = create_worker_thread(afmongodb_worker_thread, self, TRUE, NULL);
}

static void
afmongodb_dd_stop_thread (MongoDBDestDriver *self)
{
  self->writer_thread_terminate = TRUE;
  g_cond_signal(self->writer_thread_wakeup_cond);
  g_thread_join(self->writer_thread);
}

static gboolean
afmongodb_dd_init(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  gint num_keys, num_values, i;
  GList *key, *value;

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  msg_verbose("Initializing MongoDB destination",
	      evt_tag_str("host", self->host),
	      evt_tag_int("port", self->port),
	      evt_tag_str("database", self->db),
	      NULL);

  if (!self->queue)
    {
      self->queue = cfg_persist_config_fetch(cfg, afmongodb_dd_format_stats_instance(self));

      if (!self->queue)
	self->queue = log_queue_new (self->log_fifo_size);
    }

  if (!self->fields)
    {
      num_keys = g_list_length(self->keys);
      num_values = g_list_length(self->values);

      if (num_keys != num_values)
	{
	  msg_error("The number of keys and values do not match",
		    evt_tag_int("num_keys", num_keys),
		    evt_tag_int("num_values", num_values),
		    NULL);
	  return FALSE;
	}
      self->num_fields = num_keys;
      self->fields = g_new0(MongoDBField, num_keys);

      for (i = 0, key = self->keys, value = self->values; key && value; i++, key = key->next, value = value->next)
	{
	  self->fields[i].name = g_strdup(key->data);
	  self->fields[i].value = log_template_new(cfg, NULL, (gchar *)value->data);
	}
    }

  stats_register_counter(0, SCS_MONGODB | SCS_DESTINATION, self->super.id,
			 afmongodb_dd_format_stats_instance(self),
			 SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, SCS_MONGODB | SCS_DESTINATION, self->super.id,
			 afmongodb_dd_format_stats_instance(self),
			 SC_TYPE_DROPPED, &self->dropped_messages);

  afmongodb_dd_start_thread(self);

  return TRUE;
}

static gboolean
afmongodb_dd_deinit(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  afmongodb_dd_stop_thread(self);

  stats_unregister_counter(SCS_MONGODB | SCS_DESTINATION, self->super.id,
			   afmongodb_dd_format_stats_instance(self),
			   SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_MONGODB | SCS_DESTINATION, self->super.id,
			   afmongodb_dd_format_stats_instance(self),
			   SC_TYPE_DROPPED, &self->dropped_messages);

  return TRUE;
}

static void
afmongodb_dd_free(LogPipe *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;
  gint i;

  g_mutex_free(self->suspend_mutex);
  g_mutex_free(self->queue_mutex);
  g_cond_free(self->writer_thread_wakeup_cond);

  if (self->queue)
    log_queue_free(self->queue);

  for (i = 0; i < self->num_fields; i++)
    {
      g_free(self->fields[i].name);
      log_template_unref(self->fields[i].value);
    }

  g_free(self->fields);
  g_free(self->db);
  log_template_unref(self->coll);
  g_free(self->user);
  g_free(self->password);
  g_free(self->host);
  string_list_free(self->keys);
  string_list_free(self->values);

  log_drv_free(d);
}

static void
afmongodb_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  gboolean consumed, queue_was_empty;

  g_mutex_lock(self->queue_mutex);
  self->last_msg_stamp = cached_g_current_time_sec ();
  queue_was_empty = log_queue_get_length(self->queue) == 0;
  consumed = log_queue_push_tail(self->queue, msg, path_options);
  g_mutex_unlock(self->queue_mutex);

  g_mutex_lock(self->suspend_mutex);
  if (consumed && queue_was_empty && !self->writer_thread_suspended)
    g_cond_signal(self->writer_thread_wakeup_cond);
  g_mutex_unlock(self->suspend_mutex);

  if (!consumed)
    {
      stats_counter_inc(self->dropped_messages);
      msg_debug("Destination queue full, dropping message",
		evt_tag_int("queue_len", log_queue_get_length(self->queue)),
		evt_tag_int("log_fifo_size", self->log_fifo_size),
		NULL);
      log_msg_drop(msg, path_options);
    }
}

/*
 * Plugin glue.
 */

const gchar *default_keys[] =
{
  "date",
  "facility",
  "level",
  "host",
  "program",
  "pid",
  "message",
  NULL
};

const gchar *default_values[] =
{
  "${R_YEAR}-${R_MONTH}-${R_DAY} ${R_HOUR}:${R_MIN}:${R_SEC}",
  "$FACILITY",
  "$LEVEL",
  "$HOST",
  "$PROGRAM",
  "$PID",
  "$MSGONLY",
  NULL
};

LogDriver *
afmongodb_dd_new(void)
{
  MongoDBDestDriver *self = g_new0(MongoDBDestDriver, 1);

  mongo_util_oid_init (0);

  log_drv_init_instance(&self->super);
  self->super.super.init = afmongodb_dd_init;
  self->super.super.deinit = afmongodb_dd_deinit;
  self->super.super.queue = afmongodb_dd_queue;
  self->super.super.free_fn = afmongodb_dd_free;

  afmongodb_dd_set_host((LogDriver *)self, "127.0.0.1");
  afmongodb_dd_set_port((LogDriver *)self, 27017);
  afmongodb_dd_set_database((LogDriver *)self, "syslog");
  afmongodb_dd_set_collection((LogDriver *)self, "messages");
  afmongodb_dd_set_keys((LogDriver *)self, string_array_to_list(default_keys));
  afmongodb_dd_set_values((LogDriver *)self, string_array_to_list(default_values));
  afmongodb_dd_set_log_fifo_size((LogDriver *)self, 1000);

  init_sequence_number(&self->seq_num);

  self->writer_thread_wakeup_cond = g_cond_new();
  self->suspend_mutex = g_mutex_new();
  self->queue_mutex = g_mutex_new();

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
