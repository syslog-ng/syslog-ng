/*
 * Copyright (c) 2018 One Identity
 * Copyright (c) 2013, 2014, 2015 Balabit
 * Copyright (c) 2013, 2014, 2015 Gergely Nagy
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

#include "riemann.h"
#include "riemann-worker.h"
#include "scratch-buffers.h"

#include <riemann/simple.h>
#include <stdlib.h>

static void
_set_timeout_on_connection(RiemannDestWorker *self)
{
  RiemannDestDriver *owner = (RiemannDestDriver *)self->super.owner;

  int fd;
  struct timeval timeout;
  if (owner->timeout >= 1)
    {
      fd = riemann_client_get_fd(self->client);
      timeout.tv_sec = owner->timeout;
      timeout.tv_usec = 0;
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof (timeout));
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout));
    }
}

static gboolean
riemann_dd_connect(LogThreadedDestWorker *s)
{
  RiemannDestWorker *self = (RiemannDestWorker *) s;
  RiemannDestDriver *owner = (RiemannDestDriver *) s->owner;

  self->client = riemann_client_create(owner->type, owner->server, owner->port,
                                       RIEMANN_CLIENT_OPTION_TLS_CA_FILE, owner->tls.cacert,
                                       RIEMANN_CLIENT_OPTION_TLS_CERT_FILE, owner->tls.cert,
                                       RIEMANN_CLIENT_OPTION_TLS_KEY_FILE, owner->tls.key,
                                       RIEMANN_CLIENT_OPTION_NONE);
  if (!self->client)
    {
      msg_error("riemann: error connecting to Riemann server",
                evt_tag_str("server", owner->server),
                evt_tag_int("port", owner->port),
                evt_tag_error("errno"),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  _set_timeout_on_connection(self);

  return TRUE;
}

static void
riemann_dd_disconnect(LogThreadedDestWorker *s)
{
  RiemannDestWorker *self = (RiemannDestWorker *) s;

  riemann_client_disconnect(self->client);
  riemann_client_free(self->client);
  self->client = NULL;
}

/*
 * Main thread
 */


static void
riemann_dd_field_string_maybe_add(riemann_event_t *event, LogMessage *msg,
                                  LogTemplate *template, const LogTemplateOptions *template_options,
                                  riemann_event_field_t ftype,
                                  gint32 seq_num,
                                  GString *target)
{
  if (!template)
    return;

  log_template_format(template, msg, template_options, LTZ_SEND,
                      seq_num, NULL, target);

  if (target->len != 0)
    riemann_event_set(event, ftype, target->str, RIEMANN_EVENT_FIELD_NONE);
}

static void
riemann_dd_field_integer_maybe_add(riemann_event_t *event, LogMessage *msg,
                                   LogTemplate *template, const LogTemplateOptions *template_options,
                                   riemann_event_field_t ftype,
                                   gint32 seq_num,
                                   GString *target)
{
  if (!template)
    return;

  log_template_format(template, msg, template_options, LTZ_SEND,
                      seq_num, NULL, target);

  if (target->len != 0)
    {
      gint64 as_int = g_ascii_strtoll(target->str, NULL, 10);
      riemann_event_set(event, ftype, as_int, RIEMANN_EVENT_FIELD_NONE);
    }
}

static void
riemann_dd_field_add_tag(gpointer data, gpointer user_data)
{
  gchar *tag = (gchar *)data;
  riemann_event_t *event = (riemann_event_t *)user_data;

  riemann_event_tag_add(event, tag);
}

static gboolean
riemann_dd_field_add_msg_tag(const LogMessage *msg,
                             LogTagId tag_id, const gchar *tag_name,
                             gpointer user_data)
{
  riemann_event_t *event = (riemann_event_t *)user_data;

  riemann_event_tag_add(event, tag_name);

  return TRUE;
}

/* TODO escape '\0' when passing down the value */
static gboolean
riemann_dd_field_add_attribute_vp(const gchar *name,
                                  TypeHint type, const gchar *value,
                                  gsize value_len,
                                  gpointer user_data)
{
  riemann_event_t *event = (riemann_event_t *)user_data;
  riemann_attribute_t *attrib = riemann_attribute_new();

  riemann_attribute_set(attrib, name, value);
  riemann_event_attribute_add(event, attrib);

  return FALSE;
}

static gboolean
riemann_add_metric_to_event(RiemannDestWorker *self, riemann_event_t *event, LogMessage *msg, GString *str)
{
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;

  log_template_format(owner->fields.metric, msg, &owner->template_options,
                      LTZ_SEND, self->super.seq_num, NULL, str);

  if (str->len == 0)
    return TRUE;

  switch (owner->fields.metric->type_hint)
    {
    case TYPE_HINT_INT32:
    case TYPE_HINT_INT64:
    {
      gint64 i;

      if (type_cast_to_int64(str->str, &i, NULL))
        riemann_event_set(event, RIEMANN_EVENT_FIELD_METRIC_S64, i,
                          RIEMANN_EVENT_FIELD_NONE);
      else
        return !type_cast_drop_helper(owner->template_options.on_error,
                                      str->str, "int");
      break;
    }
    case TYPE_HINT_DOUBLE:
    case TYPE_HINT_STRING:
    {
      gdouble d;

      if (type_cast_to_double(str->str, &d, NULL))
        riemann_event_set(event, RIEMANN_EVENT_FIELD_METRIC_D, d,
                          RIEMANN_EVENT_FIELD_NONE);
      else
        return !type_cast_drop_helper(owner->template_options.on_error,
                                      str->str, "double");
      break;
    }
    default:
      return !type_cast_drop_helper(owner->template_options.on_error,
                                    str->str, "<unknown>");
      break;
    }
  return TRUE;
}

static gboolean
riemann_add_ttl_to_event(RiemannDestWorker *self, riemann_event_t *event, LogMessage *msg, GString *str)
{
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;
  gdouble d;

  log_template_format(owner->fields.ttl, msg, &owner->template_options,
                      LTZ_SEND, self->super.seq_num, NULL,
                      str);

  if (str->len == 0)
    return TRUE;

  if (type_cast_to_double (str->str, &d, NULL))
    riemann_event_set(event, RIEMANN_EVENT_FIELD_TTL, d,
                      RIEMANN_EVENT_FIELD_NONE);
  else
    return !type_cast_drop_helper(owner->template_options.on_error,
                                  str->str, "double");
  return TRUE;
}

static void
_append_event(RiemannDestWorker *self, riemann_event_t *event)
{
  self->event.list[self->event.n] = event;
  self->event.n++;
}

static gboolean
riemann_worker_insert_one(RiemannDestWorker *self, LogMessage *msg)
{
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;
  riemann_event_t *event;
  gboolean success = TRUE;
  GString *str;

  event = riemann_event_new();

  str = scratch_buffers_alloc();

  if (owner->fields.metric)
    {
      success = riemann_add_metric_to_event(self, event, msg, str);
    }

  if (success && owner->fields.ttl)
    {
      success = riemann_add_ttl_to_event(self, event, msg, str);
    }

  if (success)
    {
      riemann_dd_field_string_maybe_add(event, msg, owner->fields.host,
                                        &owner->template_options,
                                        RIEMANN_EVENT_FIELD_HOST,
                                        self->super.seq_num, str);
      riemann_dd_field_string_maybe_add(event, msg, owner->fields.service,
                                        &owner->template_options,
                                        RIEMANN_EVENT_FIELD_SERVICE,
                                        self->super.seq_num, str);
      riemann_dd_field_integer_maybe_add(event, msg, owner->fields.event_time,
                                         &owner->template_options,
                                         owner->fields.event_time_unit,
                                         self->super.seq_num, str);
      riemann_dd_field_string_maybe_add(event, msg, owner->fields.description,
                                        &owner->template_options,
                                        RIEMANN_EVENT_FIELD_DESCRIPTION,
                                        self->super.seq_num, str);
      riemann_dd_field_string_maybe_add(event, msg, owner->fields.state,
                                        &owner->template_options,
                                        RIEMANN_EVENT_FIELD_STATE,
                                        self->super.seq_num, str);

      if (owner->fields.tags)
        g_list_foreach(owner->fields.tags, riemann_dd_field_add_tag,
                       (gpointer)event);
      else
        log_msg_tags_foreach(msg, riemann_dd_field_add_msg_tag,
                             (gpointer)event);

      if (owner->fields.attributes)
        value_pairs_foreach(owner->fields.attributes,
                            riemann_dd_field_add_attribute_vp,
                            msg, self->super.seq_num, LTZ_SEND,
                            &owner->template_options, event);
      msg_trace("riemann: adding message to Riemann event",
                evt_tag_str("server", owner->server),
                evt_tag_int("port", owner->port),
                evt_tag_str("message", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      _append_event(self, event);
    }
  else
    riemann_event_free(event);

  return success;
}

static LogThreadedResult
riemann_worker_flush(LogThreadedDestWorker *s, LogThreadedFlushMode mode)
{
  RiemannDestWorker *self = (RiemannDestWorker *) s;
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;
  riemann_message_t *message;
  riemann_message_t *r;

  if (self->event.n == 0)
    return LTR_SUCCESS;

  message = riemann_message_new();

  riemann_message_set_events_n(message, self->event.n, self->event.list);
  r = riemann_communicate(self->client, message);

  /*
   * riemann_client_send_message_oneshot() will free self->event.list,
   * whether the send succeeds or fails. So we need to reallocate it,
   * and save as many messages as possible.
   */
  self->event.n = 0;
  self->event.list = (riemann_event_t **) malloc(sizeof (riemann_event_t *) *
                                                 MAX(1, owner->super.batch_lines));
  if (!r)
    {
      return LTR_ERROR;
    }

  msg_trace("riemann: flushing messages to Riemann server",
            evt_tag_str("server", owner->server),
            evt_tag_int("port", owner->port),
            evt_tag_int("batch_size", self->event.n),
            evt_tag_int("ok", r->ok),
            evt_tag_str("error", r->error),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super));

  if ((r->error) || (r->has_ok && !r->ok))
    {
      riemann_message_free(r);
      return LTR_ERROR;
    }
  else
    {
      riemann_message_free(r);
      return LTR_SUCCESS;
    }
}

static LogThreadedResult
_insert_single(RiemannDestWorker *self, LogMessage *msg)
{
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;

  if (!riemann_worker_insert_one(self, msg))
    {
      msg_error("riemann: error inserting message to batch, probably a type mismatch. Dropping message",
                evt_tag_str("server", owner->server),
                evt_tag_int("port", owner->port),
                evt_tag_str("message", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
                log_pipe_location_tag(&owner->super.super.super.super),
                evt_tag_str("driver", owner->super.super.super.id));

      return LTR_DROP;
    }

  return log_threaded_dest_worker_flush(&self->super, FALSE);
}

static LogThreadedResult
_insert_batch(RiemannDestWorker *self, LogMessage *msg)
{
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;

  if (!riemann_worker_insert_one(self, msg))
    {
      msg_error("riemann: error inserting message to batch, probably a type mismatch. Dropping message",
                evt_tag_str("server", owner->server),
                evt_tag_int("port", owner->port),
                evt_tag_str("message", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
                log_pipe_location_tag(&owner->super.super.super.super),
                evt_tag_str("driver", owner->super.super.super.id));

      /* in this case, we don't return RESULT_DROPPED as that would drop the
       * entire batch.  Rather, we simply don't add this message to our
       * current batch thereby dropping it.  Should we ever get rewind the
       * current batch we would log the same again.
       */
    }

  return LTR_QUEUED;
}

static LogThreadedResult
riemann_worker_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  RiemannDestWorker *self = (RiemannDestWorker *) s;
  RiemannDestDriver *owner = (RiemannDestDriver *) self->super.owner;

  if (owner->super.batch_lines <= 1)
    return _insert_single(self, msg);
  else
    return _insert_batch(self, msg);
}

static void
riemann_dw_free(LogThreadedDestWorker *s)
{
  RiemannDestWorker *self = (RiemannDestWorker *) s;

  free(self->event.list);
  if (self->client)
    riemann_client_free(self->client);
  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
riemann_dw_new(LogThreadedDestDriver *owner, gint worker_index)
{
  RiemannDestWorker *self = g_new0(RiemannDestWorker, 1);

  /* we don't support multiple workers */
  g_assert(worker_index == 0);
  log_threaded_dest_worker_init_instance(&self->super, owner, worker_index);

  self->super.connect = riemann_dd_connect;
  self->super.disconnect = riemann_dd_disconnect;
  self->super.insert = riemann_worker_insert;
  self->super.free_fn = riemann_dw_free;
  self->super.flush = riemann_worker_flush;
  self->event.list = (riemann_event_t **) malloc(sizeof (riemann_event_t *) *
                                                 MAX(1, owner->batch_lines));
  return &self->super;
}
