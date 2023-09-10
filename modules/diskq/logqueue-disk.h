/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#ifndef LOGQUEUE_DISK_H_INCLUDED
#define LOGQUEUE_DISK_H_INCLUDED

#include "logmsg/logmsg.h"
#include "logqueue.h"
#include "qdisk.h"
#include "logmsg/logmsg-serialize.h"

typedef struct _LogQueueDisk LogQueueDisk;

struct _LogQueueDisk
{
  LogQueue super;
  QDisk *qdisk;         /* disk based queue */
  /* TODO:
   * LogQueueDisk should have a separate options class, which should only contain compaction, reliable, etc...
   * Similarly, QDisk should have a separate options class, which should only contain capacity_bytes,
   * flow_control_window_bytes, etc...
   */

  struct
  {
    StatsClusterKey *capacity_sc_key;
    StatsClusterKey *disk_usage_sc_key;
    StatsClusterKey *disk_allocated_sc_key;

    StatsCounterItem *capacity;
    StatsCounterItem *disk_usage;
    StatsCounterItem *disk_allocated;
  } metrics;

  gboolean compaction;
  gboolean (*start)(LogQueueDisk *s);
  gboolean (*stop)(LogQueueDisk *s, gboolean *persistent);
  gboolean (*stop_corrupted)(LogQueueDisk *s);
};

extern QueueType log_queue_disk_type;

const gchar *log_queue_disk_get_filename(LogQueue *self);
gboolean log_queue_disk_stop(LogQueue *self, gboolean *persistent);
gboolean log_queue_disk_start(LogQueue *self);
void log_queue_disk_init_instance(LogQueueDisk *self, DiskQueueOptions *options, const gchar *qdisk_file_id,
                                  const gchar *filename, const gchar *persist_name, gint stats_level,
                                  StatsClusterKeyBuilder *driver_sck_builder,
                                  StatsClusterKeyBuilder *queue_sck_builder);
void log_queue_disk_restart_corrupted(LogQueueDisk *self);
void log_queue_disk_free_method(LogQueueDisk *self);

void log_queue_disk_update_disk_related_counters(LogQueueDisk *self);
LogMessage *log_queue_disk_read_message(LogQueueDisk *self, LogPathOptions *path_options);
LogMessage *log_queue_disk_peek_message(LogQueueDisk *self);
void log_queue_disk_drop_message(LogQueueDisk *self, LogMessage *msg, const LogPathOptions *path_options);
gboolean log_queue_disk_serialize_msg(LogQueueDisk *self, LogMessage *msg, GString *serialized);
gboolean log_queue_disk_deserialize_msg(LogQueueDisk *self, GString *serialized, LogMessage **msg);

#endif
