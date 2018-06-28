/*
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

#include <riemann/riemann-client.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "logthrdestdrv.h"
#include "string-list.h"
#include "stats/stats.h"
#include "scratch-buffers.h"
#include "riemann.h"

typedef struct
{
  LogThreadedDestDriver super;

  gchar *server;
  gint port;
  riemann_client_type_t type;
  guint timeout;

  struct
  {
    LogTemplate *host;
    LogTemplate *service;
    LogTemplate *event_time;
    gint event_time_unit;
    LogTemplate *state;
    LogTemplate *description;
    LogTemplate *metric;
    LogTemplate *ttl;
    GList *tags;
    ValuePairs *attributes;
  } fields;
  LogTemplateOptions template_options;

  struct
  {
    gchar *cacert;
    gchar *cert;
    gchar *key;
  } tls;

  riemann_client_t *client;

  struct
  {
    riemann_event_t **list;
    gint n;
    gint batch_size_max;
  } event;
} RiemannDestDriver;

/*
 * Configuration
 */
void
riemann_dd_set_server(LogDriver *d, const gchar *host)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  g_free(self->server);
  self->server = g_strdup(host);
}

void
riemann_dd_set_port(LogDriver *d, gint port)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->port = port;
}

void
riemann_dd_set_field_host(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.host = log_template_ref(value);
}

void
riemann_dd_set_field_service(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.service = log_template_ref(value);
}

void
riemann_dd_set_field_event_time(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.event_time = log_template_ref(value);
}

void riemann_dd_set_event_time_unit(LogDriver *d, gint unit)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.event_time_unit = unit;
}

void
riemann_dd_set_field_state(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.state = log_template_ref(value);
}

void
riemann_dd_set_field_description(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.description = log_template_ref(value);
}

void
riemann_dd_set_field_metric(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.metric = log_template_ref(value);
}

void
riemann_dd_set_field_ttl(LogDriver *d, LogTemplate *value)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->fields.ttl = log_template_ref(value);
}

void
riemann_dd_set_field_tags(LogDriver *d, GList *taglist)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  string_list_free(self->fields.tags);
  self->fields.tags = taglist;
}

void
riemann_dd_set_field_attributes(LogDriver *d, ValuePairs *vp)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  value_pairs_unref(self->fields.attributes);
  self->fields.attributes = vp;
}

void
riemann_dd_set_flush_lines(LogDriver *d, gint lines)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  self->event.batch_size_max = lines;
}

gboolean
riemann_dd_set_connection_type(LogDriver *d, const gchar *type)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  if (strcmp(type, "tcp") == 0)
    self->type = RIEMANN_CLIENT_TCP;
  else if (strcmp(type, "udp") == 0)
    self->type = RIEMANN_CLIENT_UDP;
  else if (strcmp(type, "tls") == 0)
    self->type = RIEMANN_CLIENT_TLS;
  else
    return FALSE;

  return TRUE;
}

void
riemann_dd_set_timeout(LogDriver *d, guint timeout)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;
  self->timeout = timeout;
}

void
riemann_dd_set_tls_cacert(LogDriver *d, const gchar *path)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  g_free(self->tls.cacert);
  self->tls.cacert = g_strdup(path);
}

void
riemann_dd_set_tls_cert(LogDriver *d, const gchar *path)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  g_free(self->tls.cert);
  self->tls.cert = g_strdup(path);
}

void
riemann_dd_set_tls_key(LogDriver *d, const gchar *path)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  g_free(self->tls.key);
  self->tls.key = g_strdup(path);
}

LogTemplateOptions *
riemann_dd_get_template_options(LogDriver *d)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  return &self->template_options;
}

/*
 * Utilities
 */

static const gchar *
riemann_dd_format_stats_instance(LogThreadedDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;
  static gchar persist_name[1024];

  if (s->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "riemann,%s", s->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "riemann,%s,%u", self->server, self->port);

  return persist_name;
}

static const gchar *
riemann_dd_format_persist_name(const LogPipe *s)
{
  const RiemannDestDriver *self = (const RiemannDestDriver *)s;
  static gchar persist_name[1024];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "riemann.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "riemann(%s,%u)", self->server, self->port);

  return persist_name;
}

static void
riemann_dd_disconnect(LogThreadedDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;

  riemann_client_disconnect(self->client);
  self->client = NULL;
}

static void
_set_timeout_on_connection(RiemannDestDriver *self)
{
  int fd;
  struct timeval timeout;
  if (self->timeout >= 1)
    {
      fd = riemann_client_get_fd(self->client);
      timeout.tv_sec = self->timeout;
      timeout.tv_usec = 0;
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof (timeout));
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout));
    }
}

static gboolean
riemann_dd_connect(RiemannDestDriver *self, gboolean reconnect)
{
  if (reconnect && self->client)
    return TRUE;

  self->client = riemann_client_create(self->type, self->server, self->port,
                                       RIEMANN_CLIENT_OPTION_TLS_CA_FILE, self->tls.cacert,
                                       RIEMANN_CLIENT_OPTION_TLS_CERT_FILE, self->tls.cert,
                                       RIEMANN_CLIENT_OPTION_TLS_KEY_FILE, self->tls.key,
                                       RIEMANN_CLIENT_OPTION_NONE);
  if (!self->client)
    {
      msg_error("Error connecting to Riemann",
                evt_tag_error("errno"));
      return FALSE;
    }

  _set_timeout_on_connection(self);

  return TRUE;
}

/*
 * Main thread
 */
static void
_value_pairs_always_exclude_properties(RiemannDestDriver *self)
{
  static const gchar *properties[] = {"host", "service", "description", "state",
                                      "ttl", "metric", "tags", NULL
                                     };
  gint i;

  if (!self->fields.attributes)
    {
      return;
    }

  for (i = 0; properties[i]; i++)
    value_pairs_add_glob_pattern(self->fields.attributes, properties[i], FALSE);
}

static gboolean
riemann_worker_init(LogPipe *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  if (!self->server)
    self->server = g_strdup("127.0.0.1");
  if (self->port == -1)
    self->port = 5555;

  if (!self->fields.host)
    {
      self->fields.host = log_template_new(cfg, NULL);
      log_template_compile(self->fields.host, "${HOST}", NULL);
    }
  if (!self->fields.service)
    {
      self->fields.service = log_template_new(cfg, NULL);
      log_template_compile(self->fields.service, "${PROGRAM}", NULL);
    }

  if (!self->fields.event_time)
    {
      self->fields.event_time = log_template_new(cfg, NULL);
      log_template_compile(self->fields.event_time, "${UNIXTIME}", NULL);
      self->fields.event_time_unit = RIEMANN_EVENT_FIELD_TIME;
    }

  _value_pairs_always_exclude_properties(self);

  if (self->event.batch_size_max <= 0)
    self->event.batch_size_max = 1;
  self->event.list = (riemann_event_t **)malloc (sizeof (riemann_event_t *) *
                                                 self->event.batch_size_max);

  msg_verbose("Initializing Riemann destination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("server", self->server),
              evt_tag_int("port", self->port));

  return log_threaded_dest_driver_init_method(s);
}

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
riemann_add_metric_to_event(RiemannDestDriver *self, riemann_event_t *event, LogMessage *msg, GString *str)
{
  log_template_format(self->fields.metric, msg, &self->template_options,
                      LTZ_SEND, self->super.seq_num, NULL, str);

  if (str->len == 0)
    return FALSE;

  switch (self->fields.metric->type_hint)
    {
    case TYPE_HINT_INT32:
    case TYPE_HINT_INT64:
    {
      gint64 i;

      if (type_cast_to_int64(str->str, &i, NULL))
        riemann_event_set(event, RIEMANN_EVENT_FIELD_METRIC_S64, i,
                          RIEMANN_EVENT_FIELD_NONE);
      else
        return type_cast_drop_helper(self->template_options.on_error,
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
        return type_cast_drop_helper(self->template_options.on_error,
                                     str->str, "double");
      break;
    }
    default:
      return type_cast_drop_helper(self->template_options.on_error,
                                   str->str, "<unknown>");
      break;
    }
  return FALSE;
};

static gboolean
riemann_add_ttl_to_event(RiemannDestDriver *self, riemann_event_t *event, LogMessage *msg, GString *str)
{
  gdouble d;

  log_template_format(self->fields.ttl, msg, &self->template_options,
                      LTZ_SEND, self->super.seq_num, NULL,
                      str);

  if (str->len == 0)
    return FALSE;

  if (type_cast_to_double (str->str, &d, NULL))
    riemann_event_set(event, RIEMANN_EVENT_FIELD_TTL, d,
                      RIEMANN_EVENT_FIELD_NONE);
  else
    return type_cast_drop_helper(self->template_options.on_error,
                                 str->str, "double");
  return FALSE;
}

static void
_append_event(RiemannDestDriver *self, riemann_event_t *event)
{
  self->event.list[self->event.n] = event;
  self->event.n++;
}

static worker_insert_result_t
riemann_worker_insert_one(RiemannDestDriver *self, LogMessage *msg)
{
  riemann_event_t *event;
  gboolean need_drop = FALSE;
  GString *str;

  event = riemann_event_new();

  str = scratch_buffers_alloc();

  if (self->fields.metric)
    {
      need_drop = riemann_add_metric_to_event(self, event, msg, str);
    }

  if (!need_drop && self->fields.ttl)
    {
      need_drop = riemann_add_ttl_to_event(self, event, msg, str);
    }

  if (!need_drop)
    {
      riemann_dd_field_string_maybe_add(event, msg, self->fields.host,
                                        &self->template_options,
                                        RIEMANN_EVENT_FIELD_HOST,
                                        self->super.seq_num, str);
      riemann_dd_field_string_maybe_add(event, msg, self->fields.service,
                                        &self->template_options,
                                        RIEMANN_EVENT_FIELD_SERVICE,
                                        self->super.seq_num, str);
      riemann_dd_field_integer_maybe_add(event, msg, self->fields.event_time,
                                         &self->template_options,
                                         self->fields.event_time_unit,
                                         self->super.seq_num, str);
      riemann_dd_field_string_maybe_add(event, msg, self->fields.description,
                                        &self->template_options,
                                        RIEMANN_EVENT_FIELD_DESCRIPTION,
                                        self->super.seq_num, str);
      riemann_dd_field_string_maybe_add(event, msg, self->fields.state,
                                        &self->template_options,
                                        RIEMANN_EVENT_FIELD_STATE,
                                        self->super.seq_num, str);

      if (self->fields.tags)
        g_list_foreach(self->fields.tags, riemann_dd_field_add_tag,
                       (gpointer)event);
      else
        log_msg_tags_foreach(msg, riemann_dd_field_add_msg_tag,
                             (gpointer)event);

      if (self->fields.attributes)
        value_pairs_foreach(self->fields.attributes,
                            riemann_dd_field_add_attribute_vp,
                            msg, self->super.seq_num, LTZ_SEND,
                            &self->template_options, event);

      _append_event(self, event);
    }

  if (need_drop)
    return WORKER_INSERT_RESULT_DROP;
  else
    return WORKER_INSERT_RESULT_SUCCESS;
}

static worker_insert_result_t
riemann_worker_batch_flush(RiemannDestDriver *self)
{
  riemann_message_t *message;
  int r;

  if (self->event.n == 0)
    return WORKER_INSERT_RESULT_SUCCESS;

  if (!riemann_dd_connect(self, TRUE))
    return WORKER_INSERT_RESULT_NOT_CONNECTED;

  message = riemann_message_new();

  riemann_message_set_events_n(message, self->event.n, self->event.list);
  r = riemann_client_send_message_oneshot(self->client, message);

  /*
   * riemann_client_send_message_oneshot() will free self->event.list,
   * whether the send succeeds or fails. So we need to reallocate it,
   * and save as many messages as possible.
   */
  self->event.n = 0;
  self->event.list = (riemann_event_t **)malloc (sizeof (riemann_event_t *) *
                                                 self->event.batch_size_max);
  if (r != 0)
    return WORKER_INSERT_RESULT_ERROR;
  else
    return WORKER_INSERT_RESULT_SUCCESS;
}

static worker_insert_result_t
riemann_worker_insert(LogThreadedDestDriver *s, LogMessage *msg)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;
  worker_insert_result_t result;

  if (self->event.n == self->event.batch_size_max)
    {
      result = riemann_worker_batch_flush(self);
      if (result != WORKER_INSERT_RESULT_SUCCESS)
        return result;
    }

  result = riemann_worker_insert_one(self, msg);

  if (self->event.n < self->event.batch_size_max)
    return result;

  return riemann_worker_batch_flush(self);
}

static void
riemann_worker_thread_deinit(LogThreadedDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;

  riemann_worker_batch_flush(self);
}

static worker_insert_result_t
riemann_flush_queue(LogThreadedDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;

  riemann_worker_batch_flush(self);

  /* NOTE: the riemann destination is clearly doing a private batching
   * implementation, which should rather be using the framework provided by
   * LogThreadedDestDriver instead.  As the interfaces of
   * LogThreadedDestDriver slightly changed, this return value is just a
   * quick port of the old behavior, however it should be ported to actually
   * use that framework not just be adapted to it.  I lack a riemann test
   * harness though, so I can't test it.
   */

  return WORKER_INSERT_RESULT_SUCCESS;
}

/*
 * Plugin glue.
 */

static void
riemann_dd_free(LogPipe *d)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  g_free(self->server);
  g_free(self->tls.cacert);
  g_free(self->tls.cert);
  g_free(self->tls.key);

  log_template_options_destroy(&self->template_options);

  riemann_client_free(self->client);

  log_template_unref(self->fields.host);
  log_template_unref(self->fields.service);
  log_template_unref(self->fields.event_time);
  log_template_unref(self->fields.state);
  log_template_unref(self->fields.description);
  log_template_unref(self->fields.metric);
  log_template_unref(self->fields.ttl);
  string_list_free(self->fields.tags);
  value_pairs_unref(self->fields.attributes);

  log_threaded_dest_driver_free(d);
}

LogDriver *
riemann_dd_new(GlobalConfig *cfg)
{
  RiemannDestDriver *self = g_new0(RiemannDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = riemann_worker_init;
  self->super.super.super.super.free_fn = riemann_dd_free;
  self->super.super.super.super.generate_persist_name = riemann_dd_format_persist_name;

  self->super.worker.disconnect = riemann_dd_disconnect;
  self->super.worker.insert = riemann_worker_insert;
  self->super.worker.thread_deinit = riemann_worker_thread_deinit;
  self->super.worker.flush = riemann_flush_queue;

  self->super.format_stats_instance = riemann_dd_format_stats_instance;
  self->super.stats_source = SCS_RIEMANN;

  self->port = -1;
  self->type = RIEMANN_CLIENT_TCP;

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}
