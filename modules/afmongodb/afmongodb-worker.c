/*
 * Copyright (c) 2010-2021 One Identity
 * Copyright (c) 2010-2014 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2021 László Várady
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

#include "afmongodb-worker.h"
#include "afmongodb-private.h"
#include "messages.h"
#include "value-pairs/evttag.h"
#include "value-pairs/value-pairs.h"

static void
_worker_disconnect(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *)s;
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (self->coll_obj)
    mongoc_collection_destroy(self->coll_obj);
  self->coll_obj = NULL;

  if (self->client)
    {
      mongoc_client_pool_push(owner->pool, self->client);
      self->client = NULL;
    }
}

static const gchar *
_format_collection_template(MongoDBDestWorker *self, LogMessage *msg)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  LogTemplateEvalOptions options = { &owner->template_options, LTZ_SEND, self->super.seq_num, NULL };
  log_template_format(owner->collection_template, msg, &options, self->collection);

  return self->collection->str;
}

static gboolean
_switch_collection(MongoDBDestWorker *self, const gchar *collection)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (!self->client)
    return FALSE;

  if (self->coll_obj)
    mongoc_collection_destroy(self->coll_obj);

  self->coll_obj = mongoc_client_get_collection(self->client, owner->const_db, collection);

  if (!self->coll_obj)
    {
      msg_error("Error getting specified MongoDB collection",
                evt_tag_str("collection", collection),
                evt_tag_str("driver", owner->super.super.super.id));

      return FALSE;
    }

  msg_debug("Switching MongoDB collection", evt_tag_str("new_collection", collection));
  return TRUE;
}

static gboolean
_check_server_status(MongoDBDestWorker *self, const mongoc_read_prefs_t *read_prefs)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  bson_t reply;
  bson_error_t error;

  if (!self->client)
    return FALSE;

  bson_t *cmd = BCON_NEW("serverStatus", "1");
  gboolean ok = mongoc_client_command_simple(self->client, owner->const_db ? : "", cmd, read_prefs, &reply, &error);
  bson_destroy(&reply);
  bson_destroy(cmd);

  if (!ok)
    {
      msg_error("Error connecting to MongoDB",
                evt_tag_str("driver", owner->super.super.super.id),
                evt_tag_str("reason", error.message));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_worker_connect(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *)s;
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (!self->client)
    {
      self->client = mongoc_client_pool_pop(owner->pool);

      if (!self->client)
        {
          msg_error("Error creating MongoDB URI",
                    evt_tag_str("driver", owner->super.super.super.id));
          return FALSE;
        }
    }

  const mongoc_read_prefs_t *read_prefs = NULL;

  if (owner->collection_is_literal_string && !self->coll_obj)
    {
      const gchar *collection = log_template_get_literal_value(owner->collection_template, NULL);

      if (!_switch_collection(self, collection))
        {
          mongoc_client_pool_push(owner->pool, self->client);
          self->client = NULL;
          return FALSE;
        }

      g_string_assign(self->collection, collection);

      read_prefs = mongoc_collection_get_read_prefs(self->coll_obj);
    }


  if (!_check_server_status(self, read_prefs))
    {
      mongoc_collection_destroy(self->coll_obj);
      self->coll_obj = NULL;
      mongoc_client_pool_push(owner->pool, self->client);
      self->client = NULL;
      return FALSE;
    }

  return TRUE;
}

/*
 * Worker thread
 */
static gboolean
_vp_obj_start(const gchar *name,
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
_vp_obj_end(const gchar *name,
            const gchar *prefix, gpointer *prefix_data,
            const gchar *prev, gpointer *prev_data,
            gpointer user_data)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) user_data;
  bson_t *root;

  if (prev_data)
    root = (bson_t *)*prev_data;
  else
    root = self->bson;

  if (prefix_data)
    {
      bson_t *d = (bson_t *)*prefix_data;

      bson_append_document(root, name, -1, d);
      bson_destroy(d);
    }
  return FALSE;
}

static gboolean
_vp_process_value(const gchar *name, const gchar *prefix, TypeHint type,
                  const gchar *value, gsize value_len, gpointer *prefix_data, gpointer user_data)
{
  bson_t *o;
  MongoDBDestWorker *self = (MongoDBDestWorker *) user_data;
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  gboolean fallback = owner->template_options.on_error & ON_ERROR_FALLBACK_TO_STRING;

  if (prefix_data)
    o = (bson_t *)*prefix_data;
  else
    o = self->bson;

  switch (type)
    {
    case TYPE_HINT_BOOLEAN:
    {
      gboolean b;

      if (type_cast_to_boolean(value, &b, NULL))
        bson_append_bool(o, name, -1, b);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, "boolean");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }
      break;
    }
    case TYPE_HINT_INT32:
    {
      gint32 i;

      if (type_cast_to_int32(value, &i, NULL))
        bson_append_int32(o, name, -1, i);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, "int32");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }
      break;
    }
    case TYPE_HINT_INT64:
    {
      gint64 i;

      if (type_cast_to_int64(value, &i, NULL))
        bson_append_int64(o, name, -1, i);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, "int64");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }

      break;
    }
    case TYPE_HINT_DOUBLE:
    {
      gdouble d;

      if (type_cast_to_double(value, &d, NULL))
        bson_append_double(o, name, -1, d);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, "double");
          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }

      break;
    }
    case TYPE_HINT_DATETIME:
    {
      guint64 i;

      if (type_cast_to_datetime_int(value, &i, NULL))
        bson_append_date_time(o, name, -1, (gint64)i);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, "datetime");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }

      break;
    }
    case TYPE_HINT_STRING:
    case TYPE_HINT_LITERAL:
      bson_append_utf8(o, name, -1, value, value_len);
      break;
    default:
      return TRUE;
    }

  return FALSE;
}

static LogThreadedResult
_worker_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  gboolean success;
  gboolean drop_silently = owner->template_options.on_error & ON_ERROR_SILENT;

  bson_reinit(self->bson);

  LogTemplateEvalOptions options = {&owner->template_options, LTZ_SEND, self->super.seq_num, NULL};
  success = value_pairs_walk(owner->vp,
                             _vp_obj_start,
                             _vp_process_value,
                             _vp_obj_end,
                             msg, &options,
                             self);

  if (!success)
    {
      if (!drop_silently)
        {
          msg_error("Failed to format message for MongoDB, dropping message",
                    evt_tag_value_pairs("message", owner->vp, msg, &options),
                    evt_tag_str("driver", owner->super.super.super.id));
        }
      return LTR_DROP;
    }

  msg_debug("Outgoing message to MongoDB destination",
            evt_tag_value_pairs("message", owner->vp, msg, &options),
            evt_tag_str("driver", owner->super.super.super.id));


  if (!owner->collection_is_literal_string)
    {
      const gchar *new_collection = _format_collection_template(self, msg);
      if (!_switch_collection(self, new_collection))
        return LTR_ERROR;
    }

  bson_error_t error;
  success = mongoc_collection_insert(self->coll_obj, MONGOC_INSERT_NONE,
                                     (const bson_t *)self->bson, NULL, &error);
  if (!success)
    {
      if (error.domain == MONGOC_ERROR_STREAM)
        {
          msg_error("Network error while inserting into MongoDB",
                    evt_tag_int("time_reopen", self->super.time_reopen),
                    evt_tag_str("reason", error.message),
                    evt_tag_str("driver", owner->super.super.super.id));
          return LTR_NOT_CONNECTED;
        }
      else
        {
          msg_error("Failed to insert into MongoDB",
                    evt_tag_int("time_reopen", self->super.time_reopen),
                    evt_tag_str("reason", error.message),
                    evt_tag_str("driver", owner->super.super.super.id));
          return LTR_ERROR;
        }
    }

  return LTR_SUCCESS;
}

static gboolean
_worker_thread_init(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;

  self->collection = g_string_sized_new(64);
  self->bson = bson_sized_new(4096);

  return log_threaded_dest_worker_init_method(s);
}

static void
_worker_thread_deinit(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;

  if (self->bson)
    bson_destroy(self->bson);
  self->bson = NULL;

  g_string_free(self->collection, TRUE);
  self->collection = NULL;

  log_threaded_dest_worker_deinit_method(s);
}

LogThreadedDestWorker *
afmongodb_dw_new(LogThreadedDestDriver *owner, gint worker_index)
{
  MongoDBDestWorker *self = g_new0(MongoDBDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, owner, worker_index);

  self->super.thread_init = _worker_thread_init;
  self->super.thread_deinit = _worker_thread_deinit;
  self->super.connect = _worker_connect;
  self->super.disconnect = _worker_disconnect;
  self->super.insert = _worker_insert;

  return &self->super;
}
