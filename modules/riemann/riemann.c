/*
 * Copyright (c) 2013, 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013, 2014 Gergely Nagy <algernon@balabit.hu>
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
#include "misc.h"
#include "stats/stats.h"
#include "scratch-buffers.h"
#include "riemann.h"

typedef struct
{
  LogThrDestDriver super;

  gchar *server;
  gint port;
  riemann_client_type_t type;

  struct
  {
    LogTemplate *host;
    LogTemplate *service;
    LogTemplate *state;
    LogTemplate *description;
    LogTemplate *metric;
    LogTemplate *ttl;
    GList *tags;
    ValuePairs *attributes;
  } fields;
  LogTemplateOptions template_options;

  riemann_client_t *client;
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

  if (self->fields.attributes)
    value_pairs_unref(self->fields.attributes);
  self->fields.attributes = vp;
}

gboolean
riemann_dd_set_connection_type(LogDriver *d, const gchar *type)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  if (strcmp(type, "tcp") == 0)
    self->type = RIEMANN_CLIENT_TCP;
  else if (strcmp(type, "udp") == 0)
    self->type = RIEMANN_CLIENT_UDP;
  else
    return FALSE;

  return TRUE;
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

static gchar *
riemann_dd_format_stats_instance(LogThrDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "riemann,%s,%u", self->server, self->port);
  return persist_name;
}

static gchar *
riemann_dd_format_persist_name(LogThrDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "riemann(%s,%u)", self->server, self->port);
  return persist_name;
}

static void
riemann_dd_disconnect(LogThrDestDriver *s)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;

  riemann_client_disconnect(self->client);
  self->client = NULL;
}

static gboolean
riemann_dd_connect(RiemannDestDriver *self, gboolean reconnect)
{
  if (reconnect && self->client)
    return TRUE;

  self->client = riemann_client_create(self->type, self->server, self->port);
  if (!self->client)
    {
      msg_error("Error connecting to Riemann",
                evt_tag_errno("errno", errno),
                NULL);
      return FALSE;
    }

  return TRUE;
}

/*
 * Main thread
 */

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

  msg_verbose("Initializing Riemann destination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("server", self->server),
              evt_tag_int("port", self->port),
              NULL);

  return log_threaded_dest_driver_start(s);
}

static void
riemann_dd_field_maybe_add(riemann_event_t *event, LogMessage *msg,
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
riemann_dd_field_add_tag(gpointer data, gpointer user_data)
{
  gchar *tag = (gchar *)data;
  riemann_event_t *event = (riemann_event_t *)user_data;

  riemann_event_tag_add(event, tag);
}

static gboolean
riemann_dd_field_add_msg_tag(LogMessage *msg,
                             LogTagId tag_id, const gchar *tag_name,
                             gpointer user_data)
{
  riemann_event_t *event = (riemann_event_t *)user_data;

  riemann_event_tag_add(event, tag_name);

  return TRUE;
}

static gboolean
riemann_dd_field_add_attribute_vp(const gchar *name,
                                  TypeHint type, const gchar *value,
                                  gpointer user_data)
{
  riemann_event_t *event = (riemann_event_t *)user_data;
  riemann_attribute_t *attrib = riemann_attribute_new();

  riemann_attribute_set(attrib, name, value);
  riemann_event_attribute_add(event, attrib);

  return FALSE;
}

static gboolean
riemann_add_metric_to_event(RiemannDestDriver *self, riemann_event_t *event, LogMessage *msg, SBGString *str)
{
  log_template_format(self->fields.metric, msg, &self->template_options,
		    LTZ_SEND, self->super.seq_num, NULL, sb_gstring_string(str));

  if (sb_gstring_string(str)->len == 0)
    return FALSE;

  switch (self->fields.metric->type_hint)
    {
      case TYPE_HINT_INT32:
      case TYPE_HINT_INT64:
	{
	  gint64 i;

	  if (type_cast_to_int64(sb_gstring_string(str)->str, &i, NULL))
	    riemann_event_set(event, RIEMANN_EVENT_FIELD_METRIC_S64, i,
			      RIEMANN_EVENT_FIELD_NONE);
	  else
	    return type_cast_drop_helper(self->template_options.on_error,
					      sb_gstring_string(str)->str, "int");
	  break;
	}
      case TYPE_HINT_DOUBLE:
      case TYPE_HINT_STRING:
	{
	  gdouble d;

	  if (type_cast_to_double(sb_gstring_string(str)->str, &d, NULL))
	    riemann_event_set(event, RIEMANN_EVENT_FIELD_METRIC_D, d,
			      RIEMANN_EVENT_FIELD_NONE);
	  else
	    return type_cast_drop_helper(self->template_options.on_error,
					      sb_gstring_string(str)->str, "double");
	  break;
	}
      default:
	return type_cast_drop_helper(self->template_options.on_error,
					  sb_gstring_string(str)->str, "<unknown>");
	break;
    }
  return FALSE;
};

static gboolean
riemann_add_ttl_to_event(RiemannDestDriver *self, riemann_event_t *event, LogMessage *msg, SBGString *str)
{
  gdouble d;

  log_template_format(self->fields.ttl, msg, &self->template_options,
		      LTZ_SEND, self->super.seq_num, NULL,
		      sb_gstring_string(str));

  if (sb_gstring_string(str)->len == 0)
    return FALSE;

  if (type_cast_to_double (sb_gstring_string(str)->str, &d, NULL))
    riemann_event_set(event, RIEMANN_EVENT_FIELD_TTL, (float) d,
		      RIEMANN_EVENT_FIELD_NONE);
  else
    return type_cast_drop_helper(self->template_options.on_error,
				      sb_gstring_string(str)->str, "double");
  return FALSE;
}

static worker_insert_result_t
riemann_worker_insert(LogThrDestDriver *s, LogMessage *msg)
{
  RiemannDestDriver *self = (RiemannDestDriver *)s;
  riemann_event_t *event;
  gboolean success = TRUE, need_drop = FALSE;
  SBGString *str;

  if (!riemann_dd_connect(self, TRUE))
    return WORKER_INSERT_RESULT_ERROR;

  event = riemann_event_new();

  str = sb_gstring_acquire();

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
      riemann_dd_field_maybe_add(event, msg, self->fields.host,
                                 &self->template_options,
                                 RIEMANN_EVENT_FIELD_HOST,
                                 self->super.seq_num, sb_gstring_string(str));
      riemann_dd_field_maybe_add(event, msg, self->fields.service,
                                 &self->template_options,
                                 RIEMANN_EVENT_FIELD_SERVICE,
                                 self->super.seq_num, sb_gstring_string(str));
      riemann_dd_field_maybe_add(event, msg, self->fields.description,
                                 &self->template_options,
                                 RIEMANN_EVENT_FIELD_DESCRIPTION,
                                 self->super.seq_num, sb_gstring_string(str));
      riemann_dd_field_maybe_add(event, msg, self->fields.state,
                                 &self->template_options,
                                 RIEMANN_EVENT_FIELD_STATE,
                                 self->super.seq_num, sb_gstring_string(str));

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

      if (riemann_client_send_message_oneshot
          (self->client,
           riemann_message_create_with_events(event, NULL)) != 0)
        success = FALSE;
    }

  sb_gstring_release(str);

  if (need_drop)
    return WORKER_INSERT_RESULT_DROP;

  if (success)
    return WORKER_INSERT_RESULT_SUCCESS;
  else
    return WORKER_INSERT_RESULT_ERROR;
}

/*
 * Plugin glue.
 */

static void
riemann_dd_free(LogPipe *d)
{
  RiemannDestDriver *self = (RiemannDestDriver *)d;

  g_free(self->server);

  log_template_options_destroy(&self->template_options);

  riemann_client_free(self->client);

  log_template_unref(self->fields.host);
  log_template_unref(self->fields.service);
  log_template_unref(self->fields.state);
  log_template_unref(self->fields.description);
  log_template_unref(self->fields.metric);
  log_template_unref(self->fields.ttl);
  string_list_free(self->fields.tags);
  if (self->fields.attributes)
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

  self->super.worker.disconnect = riemann_dd_disconnect;
  self->super.worker.insert = riemann_worker_insert;

  self->super.format.stats_instance = riemann_dd_format_stats_instance;
  self->super.format.persist_name = riemann_dd_format_persist_name;
  self->super.stats_source = SCS_RIEMANN;

  self->port = -1;
  self->type = RIEMANN_CLIENT_TCP;

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}
