/*
 * Copyright (c) 2013-2019 Balabit
 * Copyright (c) 2013 Tihamer Petrovics <tihameri@gmail.com>
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include <zlib.h>


typedef struct _KafkaDestWorker
{
  LogThreadedDestWorker super;
  GString *key;
  GString *message;
} KafkaDestWorker;

/*
 * Worker thread
 */

static LogThreadedResult
kafka_dest_worker_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  KafkaDestWorker *self = (KafkaDestWorker *)s;
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  if (owner->key)
    log_template_format(owner->key, msg, &owner->template_options, LTZ_SEND,
                        self->super.seq_num, NULL, self->key);
  log_template_format(owner->message, msg, &owner->template_options, LTZ_SEND,
                      self->super.seq_num, NULL, self->message);

  if (rd_kafka_produce(owner->topic,
                       RD_KAFKA_PARTITION_UA,
                       RD_KAFKA_MSG_F_FREE,
                       self->message->str,
                       self->message->len,
                       owner->key ? self->key->str : NULL,
                       owner->key ? self->key->len : 0,
                       msg) == -1)
    {
      msg_error("Failed to add message to Kafka topic!",
                evt_tag_str("driver", owner->super.super.super.id),
                evt_tag_str("topic", owner->topic_name),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_last_error())));
      return LTR_ERROR;
    }

  msg_debug("Kafka event sent",
            evt_tag_str("driver", owner->super.super.super.id),
            evt_tag_str("topic", owner->topic_name),
            evt_tag_str("payload", self->message->str));

  /* we passed the allocated buffers to rdkafka, which will eventually free them */
  g_string_steal(self->message);

  if (owner->key)
    g_string_steal(self->key);

  return LTR_SUCCESS;
}

static void
kafka_dest_worker_free(LogThreadedDestWorker *s)
{
  KafkaDestWorker *self = (KafkaDestWorker *)s;
  g_string_free(self->key, TRUE);
  g_string_free(self->message, TRUE);
}

LogThreadedDestWorker *
kafka_dest_worker_new(LogThreadedDestDriver *owner, gint worker_index)
{
  KafkaDestWorker *self = g_new0(KafkaDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, owner, worker_index);
  self->super.insert = kafka_dest_worker_insert;
  self->super.free_fn = kafka_dest_worker_free;

  self->key = g_string_sized_new(1024);
  self->message = g_string_sized_new(1024);
  return &self->super;
}
