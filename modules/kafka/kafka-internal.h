/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 Balazs Scheidler
 * Copyright (c) 2020 Vivin Peris
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

#ifndef KAFKA_INTERNAL_H_INCLUDED
#define KAFKA_INTERNAL_H_INCLUDED

#include "logthrdest/logthrdestdrv.h"
#include <librdkafka/rdkafka.h>
#include "kafka-dest-worker.h"

gchar *kafka_dest_worker_resolve_template_topic_name(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dest_worker_calculate_topic_from_template(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dest_worker_get_literal_topic(KafkaDestWorker *self);
rd_kafka_topic_t *kafka_dest_worker_calculate_topic(KafkaDestWorker *self, LogMessage *msg);
gboolean kafka_dd_init(LogPipe *s);

#endif

