/*
 * Copyright (c) 2021 One Identity
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

#include "mqtt-source.h"
#include <MQTTClient.h>
#include "messages.h"

#define RECEIVE_TIMEOUT 1000

void
mqtt_sd_set_topic(LogDriver *s, const gchar *topic)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *) s;
  g_free(self->topic);
  self->topic = g_strdup(topic);
}

const gchar *
_format_persist_name(const LogPipe *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *) s;
  static gchar stats_instance[1024];

  if (((LogPipe *)s)->persist_name)
    g_snprintf(stats_instance, sizeof(stats_instance), "%s", ((LogPipe *)s)->persist_name);
  else
    g_snprintf(stats_instance, sizeof(stats_instance), "mqtt,source,%s,%s", mqtt_client_options_get_address(&self->options),
               self->topic);

  return stats_instance;
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *) s;
  LogPipe *p = &s->super.super.super;
  static gchar persist_name[1024];

  if (p->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "mqtt-source.%s", p->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "mqtt-source.(%s,%s)", mqtt_client_options_get_address(&self->options),
               self->topic);

  return persist_name;
}

static LogMessage *
_create_message(MQTTSourceDriver *self, const gchar *message, gint length)
{
  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, message, length);

  return msg;
}

static gboolean
_client_init(MQTTSourceDriver *self)
{
  gint rc;

  if ((rc = MQTTClient_create(&self->client, mqtt_client_options_get_address(&self->options),
                              mqtt_client_options_get_client_id(&self->options),
                              MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
      msg_error("Error creating mqtt client",
                evt_tag_str("address", mqtt_client_options_get_address(&self->options)),
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("client_id", mqtt_client_options_get_client_id(&self->options)),
                log_pipe_location_tag(&self->super.super.super.super.super));
      return FALSE;
    }
  return TRUE;
}

static gint
_log_ssl_errors(const gchar *str, gsize len, gpointer u)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *) u;

  msg_error("MQTT TLS error", evt_tag_printf("line", "%.*s", (gint) len, str),
            log_pipe_location_tag(&self->super.super.super.super.super));
  return TRUE;
}

static gboolean
_subscribe_topic(MQTTSourceDriver *self)
{
  gint rc;
  if ((rc = MQTTClient_subscribe(self->client, self->topic,
                                 mqtt_client_options_get_qos(&self->options))) != MQTTCLIENT_SUCCESS)
    {
      msg_error("mqtt: Error while subscribing to topic",
                evt_tag_str("topic", self->topic),
                evt_tag_int("qos", mqtt_client_options_get_qos(&self->options)),
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("driver", self->super.super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static void
_thread_init(LogThreadedFetcherDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;
  _client_init(self);
}

static void
_thread_deinit(LogThreadedFetcherDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;

  MQTTClient_destroy(&self->client);
}

static gboolean
_connect(LogThreadedFetcherDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;

  gint rc;

  MQTTClient_connectOptions conn_opts;
  MQTTClient_SSLOptions ssl_opts;
  mqtt_client_options_to_mqtt_client_connection_option(&self->options, &conn_opts, &ssl_opts);

  if ((rc = MQTTClient_connect(self->client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
      msg_error("Error connecting mqtt client",
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("client_id", mqtt_client_options_get_client_id(&self->options)),
                log_pipe_location_tag(&self->super.super.super.super.super));
      return FALSE;
    }

  if (!_subscribe_topic(self))
    return FALSE;

  return TRUE;
}

static void
_disconnect(LogThreadedFetcherDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;

  MQTTClient_disconnect(self->client, MQTT_DISCONNECT_TIMEOUT);
}

static ThreadedFetchResult
_receive_result_evaluation(gint rc, MQTTClient_message *message)
{
  if (rc == MQTTCLIENT_SUCCESS && message != NULL)
    return THREADED_FETCH_SUCCESS;

  if (rc == MQTTCLIENT_TOPICNAME_TRUNCATED && message != NULL)
    return THREADED_FETCH_SUCCESS;

  if (rc == MQTTCLIENT_SUCCESS && message == NULL)
    // timeout
    return THREADED_FETCH_NO_DATA;

  return THREADED_FETCH_ERROR;
}

static LogThreadedFetchResult
_fetch(LogThreadedFetcherDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;
  ThreadedFetchResult result = THREADED_FETCH_ERROR;
  char *topicName = NULL;
  int topicLen;
  MQTTClient_message *message = NULL;
  LogMessage *msg = NULL;
  gint rc = MQTTClient_receive(self->client, &topicName, &topicLen, &message, RECEIVE_TIMEOUT);

  result = _receive_result_evaluation(rc, message);

  if (result == THREADED_FETCH_SUCCESS)
    {
      msg = _create_message(self, (gchar *)message->payload, message->payloadlen);
      MQTTClient_freeMessage(&message);
      MQTTClient_free(topicName);
    }

  if (result == THREADED_FETCH_ERROR)
    {
      msg_error("Error while receiving msg",
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("client_id", mqtt_client_options_get_client_id(&self->options)),
                log_pipe_location_tag(&self->super.super.super.super.super));
    }

  return (LogThreadedFetchResult)
  {
    result, msg
  };
}

static gboolean
_init(LogPipe *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;

  if (!self->topic)
    {
      msg_error("mqtt: the topic() argument is required for mqtt source",
                log_pipe_location_tag(&self->super.super.super.super.super));
      return FALSE;
    }

  if(!mqtt_client_options_checker(&self->options))
    return FALSE;

  if(!log_threaded_fetcher_driver_init_method(s))
    return FALSE;

  if (mqtt_client_options_get_client_id(&self->options) == NULL)
    {
      gchar *tmp_client_id;

      tmp_client_id = g_strdup_printf("syslog-ng-source-%s", self->topic);

      mqtt_client_options_set_client_id(&self->options, tmp_client_id);
      g_free(tmp_client_id);
    }

  return TRUE;
}

static gboolean
_deinit(LogPipe *s)
{
  return log_threaded_fetcher_driver_deinit_method(s);
}

static void
_free(LogPipe *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;
  mqtt_client_options_destroy(&self->options);
  g_free(self->topic);

  log_threaded_fetcher_driver_free_method(s);
}

LogDriver *
mqtt_sd_new(GlobalConfig *cfg)
{
  MQTTSourceDriver *self = g_new0(MQTTSourceDriver, 1);

  log_threaded_fetcher_driver_init_instance(&self->super, cfg);

  mqtt_client_options_defaults(&self->options);
  mqtt_client_options_set_log_ssl_error_fn(&self->options, self, _log_ssl_errors);

  self->super.super.super.super.super.init = _init;
  self->super.super.super.super.super.deinit = _deinit;
  self->super.super.super.super.super.free_fn = _free;

  self->super.connect = _connect;
  self->super.disconnect = _disconnect;
  self->super.fetch = _fetch;
  self->super.thread_init = _thread_init;
  self->super.thread_deinit = _thread_deinit;

  self->super.super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.super.format_stats_instance = _format_stats_instance;
  return &self->super.super.super.super;
}

MQTTClientOptions *
mqtt_sd_get_options(LogDriver *s)
{
  MQTTSourceDriver *self = (MQTTSourceDriver *)s;

  return &self->options;
}
