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

#include <stdio.h>

#define PUBLISH_TIMEOUT    10000L
#define DISCONNECT_TIMEOUT 10000

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

static LogThreadedResult
_mqtt_send(LogThreadedDestWorker *s, gchar *msg)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) s->owner;

  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  MQTTClient_deliveryToken token;
  gint rc;
  LogThreadedResult result = LTR_SUCCESS;

  pubmsg.payload = msg;
  pubmsg.payloadlen = (int)strlen(msg);
  pubmsg.qos = owner->qos;
  pubmsg.retained = 0;

  rc = MQTTClient_publishMessage(self->client, self->topic->str, &pubmsg, &token);
  msg_debug("Outgoing message to MQTT destination", evt_tag_str("topic", topic),
            evt_tag_str("message", msg), log_pipe_location_tag(&owner->super.super.super.super));



  result = _publish_result_evaluation (&self->super, rc);

  if (result != LTR_SUCCESS)
    return result;

  rc = MQTTClient_waitForCompletion(self->client, token, PUBLISH_TIMEOUT);
  result = _wait_result_evaluation(&self->super, rc);

  return result;
}

static LogThreadedResult
_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  LogThreadedResult result = LTR_SUCCESS;

  GString *string_to_write = g_string_new("");
  g_string_printf(string_to_write, "message=%s\n",
                  log_msg_get_value(msg, LM_V_MESSAGE, NULL));

  result = _mqtt_send(s, string_to_write->str);

  g_string_free(string_to_write, TRUE);

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

  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

  conn_opts.keepAliveInterval = owner->keepalive;
  conn_opts.cleansession = FALSE;
  if ((rc = MQTTClient_connect(self->client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
      msg_error("Error connecting mqtt client",
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("driver", self->super.owner->super.super.id),
                log_pipe_location_tag(&self->super.owner->super.super.super));
      return FALSE;
    }

  return TRUE;
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;

  MQTTClient_disconnect(self->client, DISCONNECT_TIMEOUT);
}

static gboolean
_thread_init(LogThreadedDestWorker *s)
{
  MQTTDestinationWorker *self = (MQTTDestinationWorker *)s;
  MQTTDestinationDriver *owner = (MQTTDestinationDriver *) s->owner;

  gint rc;

  if ((rc = MQTTClient_create(&self->client, owner->address->str,
                              log_pipe_get_persist_name(&owner->super.super.super.super),
                              MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
      msg_error("Error creating mqtt client",
                evt_tag_str("address", owner->address->str),
                evt_tag_str("error code", MQTTClient_strerror(rc)),
                evt_tag_str("driver", self->super.owner->super.super.id),
                log_pipe_location_tag(&self->super.owner->super.super.super));
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

  g_string_free(self->topic, TRUE);

  log_threaded_dest_worker_free_method(s);
}


LogThreadedDestWorker *
mqtt_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  MQTTDestinationWorker *self = g_new0(MQTTDestinationWorker, 1);

  self->topic = g_string_new("");

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);
  self->super.thread_init = _thread_init;
  self->super.thread_deinit = _thread_deinit;
  self->super.insert = _insert;
  self->super.free_fn = _free;
  self->super.connect = _connect;
  self->super.disconnect = _disconnect;

  return &self->super;
}
