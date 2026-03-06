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
#include "kafka-props.h"
#include "messages.h"
#include "str-utils.h"
#include <stdio.h>
#include <stdlib.h>

KafkaProperty *
kafka_property_new(const gchar *name, const gchar *value)
{
  KafkaProperty *self = g_new0(KafkaProperty, 1);

  self->name = g_strdup(name);
  self->value = g_strdup(value);
  return self;
}

void
kafka_property_free(KafkaProperty *self)
{
  g_free(self->name);
  g_free(self->value);
  g_free(self);
}

static gint
_property_name_compare(const KafkaProperty *kp1, const KafkaProperty *kp2)
{
  return g_strcmp0(kp1->name, kp2->name);
}

KafkaProperty *
kafka_property_list_find_not_empty(GList *l, const gchar *name)
{
  KafkaProperty key = { (gchar *)name, NULL };
  GList *li = g_list_find_custom(l, &key, (GCompareFunc) _property_name_compare);

  if (li == NULL)
    return NULL;

  KafkaProperty *conf_data = (KafkaProperty *)li->data;
  if (conf_data != NULL && conf_data->value != NULL && conf_data->value[0] != '\0')
    return conf_data;
  return NULL;
}

void
kafka_property_list_free(GList *l)
{
  g_list_foreach(l, (GFunc) kafka_property_free, NULL);

  g_list_free(l);
}
