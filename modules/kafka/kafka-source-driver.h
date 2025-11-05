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

#ifndef KAFKA_SOURCE_DRIVER_H_INCLUDED
#define KAFKA_SOURCE_DRIVER_H_INCLUDED

#include "syslog-ng.h"
#include "driver.h"
#include "logsource.h"

typedef struct _KafkaSourceOptions KafkaSourceOptions;
typedef struct _KafkaSourceDriver KafkaSourceDriver;

LogTemplateOptions *kafka_sd_get_template_options(LogDriver *d);

void kafka_sd_merge_config(LogDriver *d, GList *props);
gboolean kafka_sd_set_logging(LogDriver *d, const gchar *logging);
gboolean kafka_sd_set_topics(LogDriver *d, GList *topics);
void kafka_sd_set_bootstrap_servers(LogDriver *d, const gchar *bootstrap_servers);
void kafka_sd_set_log_fetch_delay(LogDriver *s, guint new_value);
void kafka_sd_set_log_fetch_limit(LogDriver *s, guint new_value);
void kafka_sd_set_poll_timeout(LogDriver *d, gint poll_timeout);
void kafka_sd_set_time_reopen(LogDriver *d, gint time_reopen);
void kafka_sd_set_do_not_use_bookmark(LogDriver *s, gboolean new_value);
void kafka_sd_set_message_ref(LogDriver *d, LogTemplate *message);

LogDriver *kafka_sd_new(GlobalConfig *cfg);

#endif
