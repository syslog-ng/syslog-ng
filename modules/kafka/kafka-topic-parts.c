/*
 * Copyright (c) 2018 Kokan <kokaipeter@gmail.com>
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
#include "kafka-topic-parts.h"
#include "messages.h"
#include "str-utils.h"
#include <stdio.h>
#include <stdlib.h>

KafkaTopicParts *
kafka_tps_new(const gchar *topic, GList *partitions)
{
  KafkaTopicParts *self = g_new0(KafkaTopicParts, 1);

  self->topic = g_strdup(topic);
  self->partitions = partitions;
  return self;
}

/* We assume both lists are sorted */
gboolean
kafka_tps_equal(gconstpointer tps1, gconstpointer tps2)
{
  GList *parts1 = ((KafkaTopicParts *)tps1)->partitions;
  GList *parts2 = ((KafkaTopicParts *)tps2)->partitions;

  if (FALSE == g_str_equal(((KafkaTopicParts *)tps1)->topic, ((KafkaTopicParts *)tps2)->topic)
      || g_list_length(parts1) != g_list_length(parts2))
    return FALSE;

  for (; parts1 && parts2; parts1 = parts1->next, parts2 = parts2->next)
    if (parts1->data != parts2->data)
      return FALSE;
  return TRUE;
}

void
kafka_tps_free(KafkaTopicParts *self)
{
  g_free(self->topic);
  g_list_free(self->partitions);
  g_free(self);
}

void
kafka_tps_list_free(GList *l)
{
  g_list_free_full(l, (GDestroyNotify) kafka_tps_free);
}
