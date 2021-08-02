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


#ifndef MQTT_DESTINATION_H_INCLUDED
#define MQTT_DESTINATION_H_INCLUDED

#include "driver.h"
#include "logthrdest/logthrdestdrv.h"

typedef struct
{
  LogThreadedDestDriver super;
  GString  *address;
  gint      keepalive;
  gint      qos;

  LogTemplate *message;
  LogTemplateOptions template_options;

  LogTemplate *topic_name;
  gchar *fallback_topic_name;
} MQTTDestinationDriver;

#define TOPIC_NAME_ERROR mqtt_topic_name_error_quark()

GQuark mqtt_topic_name_error_quark(void);

enum MQTTTopicError
{
  TOPIC_LENGTH_ZERO,
};

LogDriver *mqtt_dd_new(GlobalConfig *cfg);

void mqtt_dd_set_topic_template(LogDriver *d, LogTemplate *topic);
void mqtt_dd_set_fallback_topic(LogDriver *d, const gchar *fallback_topic);
void mqtt_dd_set_keepalive (LogDriver *d, const gint keepalive);
void mqtt_dd_set_address(LogDriver *d, const gchar *address);
void mqtt_dd_set_qos (LogDriver *d, const gint qos);
void mqtt_dd_set_message_template_ref(LogDriver *d, LogTemplate *message);

gboolean mqtt_dd_validate_topic_name(const gchar *name, GError **error);

LogTemplateOptions *mqtt_dd_get_template_options(LogDriver *s);

gboolean mqtt_dd_validate_address(const gchar *address);

#endif /* MQTT_DESTINATION_H_INCLUDED */