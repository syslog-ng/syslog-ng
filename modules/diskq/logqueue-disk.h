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
  gint64 (*get_length)(LogQueueDisk *s);
  gboolean (*push_tail)(LogQueueDisk *s, LogMessage *msg, LogPathOptions *local_options,
                        const LogPathOptions *path_options);
  void (*push_head)(LogQueueDisk *s, LogMessage *msg, const LogPathOptions *path_options);
  LogMessage *(*pop_head)(LogQueueDisk *s, LogPathOptions *path_options);
  void (*ack_backlog)(LogQueueDisk *s, guint num_msg_to_ack);
  void (*rewind_backlog)(LogQueueDisk *s, guint rewind_count);
  gboolean (*save_queue)(LogQueueDisk *s, gboolean *persistent);
  gboolean (*load_queue)(LogQueueDisk *s, const gchar *filename);
  gboolean (*start)(LogQueueDisk *s, const gchar *filename);
  void (*free_fn)(LogQueueDisk *s);
  gboolean (*is_reliable)(LogQueueDisk *s);
  LogMessage *(*read_message)(LogQueueDisk *self, LogPathOptions *path_options);
  gboolean (*write_message)(LogQueueDisk *self, LogMessage *msg);
  void (*restart)(LogQueueDisk *self, DiskQueueOptions *options);
};

extern QueueType log_queue_disk_type;

gboolean log_queue_disk_is_reliable(LogQueue *s);
const gchar *log_queue_disk_get_filename(LogQueue *self);
gboolean log_queue_disk_save_queue(LogQueue *self, gboolean *persistent);
gboolean log_queue_disk_load_queue(LogQueue *self, const gchar *filename);
void log_queue_disk_init_instance(LogQueueDisk *self, const gchar *persist_name);
void log_queue_disk_restart_corrupted(LogQueueDisk *self);

#endif
