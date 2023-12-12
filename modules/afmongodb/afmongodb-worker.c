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
#include "scratch-buffers.h"
#include "value-pairs/evttag.h"
#include "value-pairs/value-pairs.h"
#include "scanner/list-scanner/list-scanner.h"

static LogThreadedResult _do_bulk_flush(MongoDBDestWorker *self);

static void
_compose_bulk_op_options(MongoDBDestWorker *self)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (owner->use_bulk)
    {
      self->bson_opts = bson_new();
      bson_t def_opts = BSON_INITIALIZER;
      *self->bson_opts = def_opts;

      if (!BSON_APPEND_BOOL(self->bson_opts, "ordered", false == owner->bulk_unordered))
        msg_error("Error setting bulk option",
                  evt_tag_str("option", "ordered"),
                  evt_tag_str("driver", owner->super.super.super.id));

      if (!mongoc_write_concern_append(self->write_concern, self->bson_opts))
        msg_error("Error setting bulk option",
                  evt_tag_str("option", "write_concern"),
                  evt_tag_str("driver", owner->super.super.super.id));
    }
}

static void
_destroy_bulk_op_options(MongoDBDestWorker *self)
{
  if (self->bson_opts)
    {
      bson_destroy(self->bson_opts);
      self->bson_opts = NULL;
    }
}

static void
_compose_write_concern(MongoDBDestWorker *self)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  self->write_concern = mongoc_write_concern_new();
  mongoc_write_concern_set_w(self->write_concern, owner->write_concern_level);
}

static void
_destroy_write_concern(MongoDBDestWorker *self)
{
  if (self->write_concern)
    {
      mongoc_write_concern_destroy(self->write_concern);
      self->write_concern = NULL;
    }
}

static void
_worker_disconnect(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *)s;
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (self->bulk_op)
    {
      mongoc_bulk_operation_destroy(self->bulk_op);
      self->bulk_op = NULL;
    }

  if (self->coll_obj)
    {
      mongoc_collection_destroy(self->coll_obj);
      self->coll_obj = NULL;
    }

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

  LogTemplateEvalOptions options = { &owner->template_options, LTZ_SEND, self->super.seq_num, NULL, LM_VT_STRING };
  log_template_format(owner->collection_template, msg, &options, self->collection);

  return self->collection->str;
}

static gboolean
_switch_collection(MongoDBDestWorker *self, const gchar *collection)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (!self->client)
    return FALSE;

  if (self->bulk_op && _do_bulk_flush(self) != LTR_SUCCESS)
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
      _worker_disconnect(s);
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
_vp_process_value(const gchar *name, const gchar *prefix, LogMessageValueType type,
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
    case LM_VT_BOOLEAN:
    {
      gboolean b;

      if (type_cast_to_boolean(value, value_len, &b, NULL))
        bson_append_bool(o, name, -1, b);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, value_len, "boolean");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }
      break;
    }
    case LM_VT_INTEGER:
    {
      gint64 i;

      if (type_cast_to_int64(value, value_len, &i, NULL))
        {
          if (G_MININT32 <= i && i <= G_MAXINT32)
            bson_append_int32(o, name, -1, i);
          else
            bson_append_int64(o, name, -1, i);
        }
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, value_len, "integer");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }

      break;
    }
    case LM_VT_DOUBLE:
    {
      gdouble d;

      if (type_cast_to_double(value, value_len, &d, NULL))
        bson_append_double(o, name, -1, d);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, value_len, "double");
          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }

      break;
    }
    case LM_VT_DATETIME:
    {
      gint64 msec;

      if (type_cast_to_datetime_msec(value, value_len, &msec, NULL))
        bson_append_date_time(o, name, -1, msec);
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, value_len, "datetime");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }

      break;
    }
    case LM_VT_STRING:
      bson_append_utf8(o, name, -1, value, value_len);
      break;
    case LM_VT_LIST:
    {
      bson_t array;
      ListScanner scanner;
      gint i = 0;

      bson_append_array_begin(o, name, -1, &array);

      list_scanner_init(&scanner);
      list_scanner_input_string(&scanner, value, value_len);
      while (list_scanner_scan_next(&scanner))
        {
          gchar buf[32];
          const gchar *index_string;

          bson_uint32_to_string(i, &index_string, buf, sizeof(buf));
          bson_append_utf8(&array, index_string, -1, list_scanner_get_current_value(&scanner), -1);
          i++;
        }

      list_scanner_deinit(&scanner);
      bson_append_array_end(o, &array);
      break;
    }
    case LM_VT_JSON:
    {
      bson_t embedded_bson;

      if (bson_init_from_json(&embedded_bson, value, value_len, NULL))
        {
          bson_append_document(o, name, -1, &embedded_bson);
          bson_destroy(&embedded_bson);
        }
      else
        {
          gboolean r = type_cast_drop_helper(owner->template_options.on_error, value, value_len, "json");

          if (fallback)
            bson_append_utf8(o, name, -1, value, value_len);
          else
            return r;
        }
      break;
    }
    case LM_VT_NULL:
    {
      bson_append_null(o, name, -1);
      break;
    }
    case LM_VT_BYTES:
    case LM_VT_PROTOBUF:
    {
      bson_append_binary(o, name, -1, BSON_SUBTYPE_BINARY, (const uint8_t *) value, value_len);
      break;
    }
    default:
      return TRUE;
    }

  return FALSE;
}

static LogThreadedResult
_do_bulk_flush(MongoDBDestWorker *self)
{
  /* Take care, _worker_batch_flush -> _do_bulk_flush is called at thread shutdown as well
     at that time not neccessarily we have an inprogress bulk operation
  */
  if (self->bulk_op)
    {
      bson_error_t error;
      bson_t reply;

      int result = mongoc_bulk_operation_execute(self->bulk_op, &reply, &error);

      bson_destroy (&reply);
      mongoc_bulk_operation_destroy(self->bulk_op);
      self->bulk_op = NULL;

      if (result == 0)
        {
          MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;
          msg_error("Error while bulk inserting into MongoDB",
                    evt_tag_int("time_reopen", self->super.time_reopen),
                    evt_tag_str("reason", error.message),
                    evt_tag_str("driver", owner->super.super.super.id));
          return LTR_ERROR;
        }
    }
  return LTR_SUCCESS;
}

static LogThreadedResult
_worker_batch_flush(LogThreadedDestWorker *s, LogThreadedFlushMode expedite)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;
  return _do_bulk_flush(self);
}

static LogThreadedResult
_bulk_insert(MongoDBDestWorker *self)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  if (self->bulk_op == NULL)
    self->bulk_op = mongoc_collection_create_bulk_operation_with_opts(self->coll_obj, self->bson_opts);
  if (self->bulk_op == NULL)
    {
      msg_error("Failed to create MongoDB bulk operation",
                evt_tag_int("time_reopen", self->super.time_reopen),
                evt_tag_str("driver", owner->super.super.super.id));
      return LTR_ERROR;
    }

  mongoc_bulk_operation_set_bypass_document_validation(self->bulk_op, owner->bulk_bypass_validation);
  mongoc_bulk_operation_insert(self->bulk_op, (const bson_t *)self->bson);
  return LTR_QUEUED;
}

static LogThreadedResult
_single_insert(MongoDBDestWorker *self)
{
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  bson_error_t error;
  bool success = mongoc_collection_insert(self->coll_obj, MONGOC_INSERT_NONE,
                                          (const bson_t *)self->bson, self->write_concern, &error);
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

static LogThreadedResult
_worker_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;
  MongoDBDestDriver *owner = (MongoDBDestDriver *) self->super.owner;

  gboolean success;
  gboolean drop_silently = owner->template_options.on_error & ON_ERROR_SILENT;

  bson_reinit(self->bson);

  LogTemplateEvalOptions options = {&owner->template_options, LTZ_SEND, self->super.seq_num, NULL, LM_VT_STRING};
  success = value_pairs_walk(owner->vp,
                             _vp_obj_start,
                             _vp_process_value,
                             _vp_obj_end,
                             msg, &options,
                             0,
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
      ScratchBuffersMarker mark;
      GString *last_collection = scratch_buffers_alloc_and_mark(&mark);
      g_string_assign(last_collection, self->collection->str);
      const gchar *new_collection = _format_collection_template(self, msg);
      bool should_switch_collection = (strcmp(last_collection->str, new_collection) != 0);
      scratch_buffers_reclaim_marked(mark);

      if (should_switch_collection && !_switch_collection(self, new_collection))
        return LTR_ERROR;
    }

  if (owner->use_bulk)
    return _bulk_insert(self);
  else
    return _single_insert(self);
}

static gboolean
_worker_init(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;

  self->collection = g_string_sized_new(64);
  self->bson = bson_sized_new(4096);
  /* NOTE: write concern can be used by _compose_bulk_op_options too, keep the order! */
  _compose_write_concern(self);
  _compose_bulk_op_options(self);

  return log_threaded_dest_worker_init_method(s);
}

static void
_worker_deinit(LogThreadedDestWorker *s)
{
  MongoDBDestWorker *self = (MongoDBDestWorker *) s;

  _destroy_write_concern(self);
  _destroy_bulk_op_options(self);

  if (self->bson)
    bson_destroy(self->bson);
  self->bson = NULL;

  g_string_free(self->collection, TRUE);
  self->collection = NULL;

  log_threaded_dest_worker_deinit_method(s);
}

LogThreadedDestWorker *
afmongodb_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  MongoDBDestWorker *self = g_new0(MongoDBDestWorker, 1);
  MongoDBDestDriver *owner = (MongoDBDestDriver *) o;

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);

  self->super.init = _worker_init;
  self->super.deinit = _worker_deinit;
  self->super.connect = _worker_connect;
  self->super.disconnect = _worker_disconnect;
  self->super.insert = _worker_insert;
  if (owner->use_bulk)
    self->super.flush = _worker_batch_flush;

  return &self->super;
}
