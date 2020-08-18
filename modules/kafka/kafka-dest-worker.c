/*
 * Copyright (c) 2013 Tihamer Petrovics <tihameri@gmail.com>
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Balazs Scheidler
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
#include "kafka-dest-worker.h"
#include "kafka-dest-driver.h"
#include "str-utils.h"
#include "timeutils/misc.h"
#include <zlib.h>

static gboolean
_is_poller_thread(KafkaDestWorker *self)
{
  return (self->super.worker_index == 0);
}

static void
_format_message_and_key(KafkaDestWorker *self, LogMessage *msg)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  log_template_format(owner->message, msg, &owner->template_options, LTZ_SEND,
                      self->super.seq_num, NULL, self->message);

  if (owner->key)
    log_template_format(owner->key, msg, &owner->template_options, LTZ_SEND,
                        self->super.seq_num, NULL, self->key);
}

const gchar *
kafka_dest_worker_resolve_template_topic_name(KafkaDestWorker *self, LogMessage *msg)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;
  log_template_format(owner->topic_name, msg, &owner->template_options, LTZ_SEND, self->super.seq_num, NULL,
                      self->topic_name_buffer);

  GError *error = NULL;

  if (kafka_dd_validate_topic_name(self->topic_name_buffer->str, &error))
    {
      return self->topic_name_buffer->str;
    }

  msg_error("Error constructing topic", evt_tag_str("topic_name", self->topic_name_buffer->str),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super),
            evt_tag_str("error message", error->message));

  g_error_free(error);

  return owner->fallback_topic_name;
}

rd_kafka_topic_t *
kafka_dest_worker_calculate_topic_from_template(KafkaDestWorker *self, LogMessage *msg)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;
  rd_kafka_topic_t *topic = kafka_dd_query_insert_topic(owner, kafka_dest_worker_resolve_template_topic_name(self, msg));

  g_assert(topic);

  return topic;
}

rd_kafka_topic_t *
kafka_dest_worker_get_literal_topic(KafkaDestWorker *self)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  return owner->topic;
}

rd_kafka_topic_t *
kafka_dest_worker_calculate_topic(KafkaDestWorker *self, LogMessage *msg)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  if (kafka_dd_is_topic_name_a_template(owner))
    {
      return kafka_dest_worker_calculate_topic_from_template(self, msg);
    }

  return kafka_dest_worker_get_literal_topic(self);
}

static gboolean
_publish_message(KafkaDestWorker *self, LogMessage *msg)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;
  int block_flag = _is_poller_thread(self) ? 0 : RD_KAFKA_MSG_F_BLOCK;
  rd_kafka_topic_t *topic = kafka_dest_worker_calculate_topic(self, msg);

  if (rd_kafka_produce(topic,
                       RD_KAFKA_PARTITION_UA,
                       RD_KAFKA_MSG_F_FREE | block_flag,
                       self->message->str, self->message->len,
                       self->key->len ? self->key->str : NULL, self->key->len,
                       log_msg_ref(msg)) == -1)
    {
      msg_error("kafka: failed to publish message",
                evt_tag_str("topic", rd_kafka_topic_name(topic)),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_last_error())),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));

      return FALSE;
    }

  msg_debug("kafka: message published",
            evt_tag_str("topic", rd_kafka_topic_name(topic)),
            evt_tag_str("key", self->key->len ? self->key->str : "NULL"),
            evt_tag_str("message", self->message->str),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super));

  /* we passed the allocated buffers to rdkafka, which will eventually free them */
  g_string_steal(self->message);
  return TRUE;
}

static void
_update_drain_timer(KafkaDestWorker *self)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  if (!_is_poller_thread(self))
    return;

  if (iv_timer_registered(&self->poll_timer))
    iv_timer_unregister(&self->poll_timer);
  iv_validate_now();
  self->poll_timer.expires = iv_now;
  timespec_add_msec(&self->poll_timer.expires, owner->poll_timeout);
  iv_timer_register(&self->poll_timer);
}

static void
_drain_responses(KafkaDestWorker *self)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  /* we are only draining responses in the first worker thread, so all
   * callbacks originate from the same thread and we don't need to
   * synchronize between workers */
  if (!_is_poller_thread(self))
    return;

  gint count = rd_kafka_poll(owner->kafka, 0);
  if (count != 0)
    {
      msg_trace("kafka: destination side rd_kafka_poll() processed some responses",
                kafka_dd_is_topic_name_a_template(owner) ? evt_tag_str("template", owner->topic_name->template):
                evt_tag_str("topic", owner->topic_name->template),
                evt_tag_str("fallback_topic", owner->fallback_topic_name),
                evt_tag_int("count", count),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
    }
  _update_drain_timer(self);
}

/*
 * Worker thread
 */

static LogThreadedResult
kafka_dest_worker_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  KafkaDestWorker *self = (KafkaDestWorker *)s;

  _drain_responses(self);

  _format_message_and_key(self, msg);
  if (!_publish_message(self, msg))
    return LTR_RETRY;

  _drain_responses(self);
  return LTR_SUCCESS;
}

static void
kafka_dest_worker_free(LogThreadedDestWorker *s)
{
  KafkaDestWorker *self = (KafkaDestWorker *)s;
  g_string_free(self->key, TRUE);
  g_string_free(self->message, TRUE);
  g_string_free(self->topic_name_buffer, TRUE);
  log_threaded_dest_worker_free_method(s);
}

static gboolean
_thread_init(LogThreadedDestWorker *s)
{
  KafkaDestWorker *self = (KafkaDestWorker *) s;
  if (_is_poller_thread(self))
    {
      KafkaDestDriver *owner = (KafkaDestDriver *) s->owner;
      s->time_reopen = owner->poll_timeout / 1000;
    }

  return log_threaded_dest_worker_init_method(s);
}

LogThreadedDestWorker *
kafka_dest_worker_new(LogThreadedDestDriver *o, gint worker_index)
{
  KafkaDestWorker *self = g_new0(KafkaDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);
  self->super.thread_init = _thread_init;
  self->super.insert = kafka_dest_worker_insert;
  self->super.free_fn = kafka_dest_worker_free;

  IV_TIMER_INIT(&self->poll_timer);
  self->poll_timer.cookie = self;
  self->poll_timer.handler = (void (*)(void *)) _drain_responses;

  self->key = g_string_sized_new(0);
  self->message = g_string_sized_new(1024);
  self->topic_name_buffer = g_string_sized_new(256);

  return &self->super;
}
