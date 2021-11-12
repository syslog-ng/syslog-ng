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


#include "mqtt-worker.h"
#include "mqtt-destination.h"
#include "thread-utils.h"
#include "apphook.h"
#include "messages.h"

#include <stdio.h>

#define PUBLISH_TIMEOUT    10000L

static LogThreadedResult
_publish_result_evaluation (LogThreadedDestWorker *self, gint result)
{
  switch(result)
    {
    case MQTTCLIENT_SUCCESS:
      return LTR_SUCCESS;

    case MQTTCLIENT_DISCONNECTED:
      msg_error("Disconnected during publish!",
                log_pipe_location_tag(&self->owner->super.super.super));
      return LTR_NOT_CONNECTED;

    case MQTTCLIENT_MAX_MESSAGES_INFLIGHT:
      msg_error("Max message inflight! (publish)",
                log_pipe_location_tag(&self->owner->super.super.super));
      return LTR_ERROR;

    case MQTTCLIENT_FAILURE:
      msg_error("Failure during publishing!",
                log_pipe_location_tag(&self->owner->super.super.super));
      return LTR_ERROR;

    case MQTTCLIENT_PERSISTENCE_ERROR:
      g_assert_not_reached();
    case MQTTCLIENT_BAD_QOS:
      g_assert_not_reached();
    case MQTTCLIENT_BAD_STRUCTURE:
      g_assert_not_reached();
    default:
      g_assert_not_reached();

    case MQTTCLIENT_NULL_PARAMETER:
    case MQTTCLIENT_BAD_UTF8_STRING:
      msg_error("An unrecoverable error occurred during publish, dropping message.",
                evt_tag_str("error code", MQTTClient_strerror(result)),
                log_pipe_location_tag(&self->owner->super.super.super));
      return LTR_DROP;
    }
}

static LogThreadedResult
_wait_result_evaluation(LogThreadedDestWorker *self, gint result)
{
  switch(result)
    {
    case MQTTCLIENT_SUCCESS:
      return LTR_SUCCESS;

    case MQTTCLIENT_DISCONNECTED:
      msg_error("Disconnected while waiting the response!",
                log_pipe_location_tag(&self->owner->super.super.super));
      return LTR_NOT_CONNECTED;

    case MQTTCLIENT_FAILURE:
    default:
      msg_error("Error while waiting the response!",
                evt_tag_str("error code", MQTTClient_strerror(result)),
                log_pipe_location_tag(&self->owner->super.super.super));
      return LTR_ERROR;
    }
}

static const gchar *
mqtt_dest_worker_resolve_template_topic_name(MQTTDestinationWorker *self, LogMessage *msg)
{
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) self->super.owner;

  LogTemplateEvalOptions options = {&owner->template_options, LTZ_SEND, self->super.seq_num, NULL};
  log_template_format(owner->topic_name, msg, &options, self->topic_name_buffer);

  GError *error = NULL;

  if (mqtt_dd_validate_topic_name(self->topic_name_buffer->str, &error))
    {
      return self->topic_name_buffer->str;
    }

  msg_error("Error constructing topic", evt_tag_str("topic_name", self->topic_name_buffer->str),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super),
            evt_tag_str("error message", error->message));

  g_error_free(error);

  return owner->fallback_topic;
}

static LogThreadedResult
_mqtt_send(LogThreadedDestWorker *s, gchar *msg, const gchar *topic)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) s->owner;

  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  MQTTClient_deliveryToken token;
  gint rc;
  LogThreadedResult result = LTR_SUCCESS;

  pubmsg.payload = msg;
  pubmsg.payloadlen = (int)strlen(msg);
  pubmsg.qos = mqtt_client_options_get_qos(&owner->options);
  pubmsg.retained = 0;

  rc = MQTTClient_publishMessage(self->client, topic, &pubmsg, &token);
  msg_debug("Outgoing message to MQTT destination", evt_tag_str("topic", topic),
            evt_tag_str("message", msg), log_pipe_location_tag(&owner->super.super.super.super));

  result = _publish_result_evaluation (&self->super, rc);

  if (result != LTR_SUCCESS)
    return result;

  rc = MQTTClient_waitForCompletion(self->client, token, PUBLISH_TIMEOUT);
  result = _wait_result_evaluation(&self->super, rc);

  return result;
}

static void
_format_message(LogThreadedDestWorker *s, LogMessage *msg)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) self->super.owner;

  LogTemplateEvalOptions options = {&owner->template_options, LTZ_SEND, self->super.seq_num, NULL};

  log_template_format(owner->message, msg, &options, self->string_to_write);
}

static LogThreadedResult
_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  LogThreadedResult result = LTR_SUCCESS;

  _format_message(s, msg);

  result = _mqtt_send(s, self->string_to_write->str, mqtt_dest_worker_resolve_template_topic_name(self, msg));

  return result;
  /*
   * LTR_DROP,
   * LTR_ERROR,
   * LTR_SUCCESS,
   * LTR_QUEUED,
   * LTR_NOT_CONNECTED,
   * LTR_RETRY,
  */
}

static gboolean
_connect(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) s->owner;

  gint rc;

  MQTTClient_connectOptions conn_opts;
  MQTTClient_SSLOptions ssl_opts;
  mqtt_client_options_to_mqtt_client_connection_option(&owner->options, &conn_opts, &ssl_opts);

  if ((rc = MQTTClient_connect(self->client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
      msg_error("Error connecting mqtt client",
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("client_id", mqtt_client_options_get_client_id(&owner->options)),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;

  MQTTClient_disconnect(self->client, MQTT_DISCONNECT_TIMEOUT);
}

static gboolean
_thread_init(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) s->owner;

  gint rc;

  if ((rc = MQTTClient_create(&self->client, mqtt_client_options_get_address(&owner->options),
                              mqtt_client_options_get_client_id(&owner->options),
                              MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
      msg_error("Error creating mqtt client",
                evt_tag_str("address", mqtt_client_options_get_address(&owner->options)),
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("client_id", mqtt_client_options_get_client_id(&owner->options)),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  return log_threaded_dest_worker_init_method(s);
}

static void
_thread_deinit(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;

  MQTTClient_destroy(&self->client);

  log_threaded_dest_worker_deinit_method(s);
}

static void
_free(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;

  g_string_free(self->string_to_write, TRUE);
  g_string_free(self->topic_name_buffer, TRUE);

  log_threaded_dest_worker_free_method(s);
}


LogThreadedDestWorker *
mqtt_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  MQTTDestinationWorker *self = g_new0(MQTTDestinationWorker, 1);

  self->string_to_write = g_string_new("");
  self->topic_name_buffer = g_string_new("");

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);
  self->super.thread_init = _thread_init;
  self->super.thread_deinit = _thread_deinit;
  self->super.insert = _insert;
  self->super.free_fn = _free;
  self->super.connect = _connect;
  self->super.disconnect = _disconnect;

  return &self->super;
}
