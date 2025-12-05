/*
 * Copyright (c) 2025 Hofi <hofione@gmail.com>
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

#ifndef KAFKA_SOURCE_PERSIST_H
#define KAFKA_SOURCE_PERSIST_H

#include "syslog-ng.h"
#include "persist-state.h"
#include "kafka-source-driver.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <librdkafka/rdkafka.h>
#pragma GCC diagnostic pop

typedef struct _KafkaSourcePersist KafkaSourcePersist;

KafkaSourcePersist *kafka_source_persist_new(KafkaSourceDriver *owner);
void kafka_source_persist_ref(KafkaSourcePersist *self, gboolean lock);
void kafka_source_persist_unref(KafkaSourcePersist *self);
gboolean kafka_source_persist_init(KafkaSourcePersist *self,
                                   PersistState *state,
                                   const gchar *topic,
                                   int32_t partition,
                                   int64_t override_position,
                                   gboolean use_offset_tracker);
void kafka_source_persist_invalidate(KafkaSourcePersist *self);
gboolean kafka_source_persist_is_ready(KafkaSourcePersist *self);
gboolean kafka_source_persist_matching(KafkaSourcePersist *self,
                                       const gchar *topic,
                                       int32_t partition);

void kafka_source_persist_fill_bookmark(KafkaSourcePersist *self,
                                        Bookmark *bookmark,
                                        int64_t offset);
void kafka_source_persist_load_position(KafkaSourcePersist *self,
                                        int64_t *offset);

#endif
