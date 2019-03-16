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

#include <zlib.h>


typedef struct _KafkaDestWorker
{
  LogThreadedDestWorker super;
} KafkaDestWorker;

static u_int32_t
kafka_calculate_partition_key(KafkaDestWorker *self, LogMessage *msg)
{
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;
  u_int32_t key;
  GString *field = g_string_sized_new(1024);

  log_template_format(owner->field, msg, &owner->template_options,
			LTZ_SEND, owner->seq_num, NULL, field);

  msg_debug("Kafka dynamic key",
	    evt_tag_str("key", field->str),
	    evt_tag_str("driver", owner->super.super.super.id),
	    NULL);

  key = crc32(0L, Z_NULL, 0);
  key = crc32(key, (const u_char *)field->str, field->len);

  msg_debug("Kafka field crc32",
            evt_tag_str("payload", field->str),
	    evt_tag_int("length", field->len),
	    evt_tag_int("crc32", key),
	    evt_tag_str("driver", owner->super.super.super.id),
	    NULL);

  g_string_free(field, TRUE);
  return key;
}

/*
 * Worker thread
 */

static LogThreadedResult
kafka_dest_worker_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  KafkaDestWorker *self = (KafkaDestWorker *)s;
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;
  u_int32_t key;

  log_template_format(owner->payload, msg, &owner->template_options, LTZ_SEND,
                      owner->seq_num, NULL, owner->payload_str);

  switch (owner->partition_type)
    {
    case PARTITION_RANDOM:
      key =  rand();
      break;
    case PARTITION_FIELD:
      key = kafka_calculate_partition_key(self, msg);
      break;
    default:
      key = 0;
    }

#define KAFKA_INITIAL_ERROR_CODE -12345
  rd_kafka_resp_err_t err = KAFKA_INITIAL_ERROR_CODE;
  if (rd_kafka_produce(owner->topic,
                       RD_KAFKA_PARTITION_UA,
                       RD_KAFKA_MSG_F_COPY,
                       owner->payload_str->str,
                       owner->payload_str->len,
                       &key, sizeof(key),
                       &err) == -1)
    {
      msg_error("Failed to add message to Kafka topic!",
                evt_tag_str("driver", owner->super.super.super.id),
                evt_tag_str("topic", owner->topic_name),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_errno2err(errno))),
                NULL);
      return LTR_ERROR;
    }

  msg_debug("Kafka produce done",
            evt_tag_str("driver", owner->super.super.super.id),
            NULL);

  /* Wait for completion. */
  if (owner->flags & KAFKA_FLAG_SYNC)
    {
      while (err == KAFKA_INITIAL_ERROR_CODE)
        {
          rd_kafka_poll(owner->kafka, 5000);
          if (err == KAFKA_INITIAL_ERROR_CODE)
            {
              msg_debug("Kafka producer is freezed",
                        evt_tag_str("driver", owner->super.super.super.id),
                        evt_tag_str("topic", owner->topic_name),
                        NULL);
            }
        }
      if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
        {
          msg_error("Failed to add message to Kafka topic!",
                    evt_tag_str("driver", owner->super.super.super.id),
                    evt_tag_str("topic", owner->topic_name),
                    evt_tag_str("error", rd_kafka_err2str(err)),
                    NULL);
          return LTR_ERROR;
        }
    }

  msg_debug("Kafka event sent",
            evt_tag_str("driver", owner->super.super.super.id),
            evt_tag_str("topic", owner->topic_name),
            evt_tag_str("payload", owner->payload_str->str),
            NULL);

  return LTR_SUCCESS;
}

static void
kafka_dest_worker_thread_init(LogThreadedDestWorker *d)
{
  KafkaDestWorker *self = (KafkaDestWorker *)d;
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  owner->payload_str = g_string_sized_new(1024);

}

static void
kafka_dest_worker_thread_deinit(LogThreadedDestWorker *d)
{
  KafkaDestWorker *self = (KafkaDestWorker *)d;
  KafkaDestDriver *owner = (KafkaDestDriver *) self->super.owner;

  g_string_free(owner->payload_str, TRUE);
}


LogThreadedDestWorker *
kafka_dest_worker_new(LogThreadedDestDriver *owner, gint worker_index)
{
  KafkaDestWorker *self = g_new0(KafkaDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, owner, worker_index);
  self->super.thread_init = kafka_dest_worker_thread_init;
  self->super.thread_deinit = kafka_dest_worker_thread_deinit;

  self->super.insert = kafka_dest_worker_insert;
  return &self->super;
}
