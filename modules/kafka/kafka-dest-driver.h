/*
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
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

#ifndef KAFKA_H_INCLUDED
#define KAFKA_H_INCLUDED

#include "logthrdest/logthrdestdrv.h"
#include <librdkafka/rdkafka.h>

typedef struct
{
  LogThreadedDestDriver super;

  LogTemplateOptions template_options;
  LogTemplate *field;
  LogTemplate *payload;

  gint32 flags;

  gchar *topic_name;
  GList *global_config;
  GList *topic_config;
  rd_kafka_topic_t *topic;
  rd_kafka_t *kafka;
  enum
  {
    PARTITION_RANDOM = 0,
    PARTITION_FIELD = 1
  } partition_type;
} KafkaDestDriver;

#define KAFKA_FLAG_NONE 0
#define KAFKA_FLAG_SYNC 0x0001

void kafka_dd_set_topic(LogDriver *d, const gchar *topic);
void kafka_dd_set_global_config(LogDriver *d, GList *props);
void kafka_dd_set_topic_config(LogDriver *d, GList *props);

void kafka_dd_set_partition_field(LogDriver *d, LogTemplate *key_field);
void kafka_dd_set_partition_random(LogDriver *d);
void kafka_dd_set_payload(LogDriver *d, LogTemplate *payload);
void kafka_dd_set_flag_sync(LogDriver *d);
LogTemplateOptions *kafka_dd_get_template_options(LogDriver *d);

LogDriver *kafka_dd_new(GlobalConfig *cfg);

#endif
