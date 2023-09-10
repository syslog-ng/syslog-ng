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

#ifndef QDISK_H_
#define QDISK_H_

#include "syslog-ng.h"
#include "diskq-options.h"

#define LOG_PATH_OPTIONS_FOR_BACKLOG GINT_TO_POINTER(0x80000000)
#define QDISK_RESERVED_SPACE 4096
#define LOG_PATH_OPTIONS_TO_POINTER(lpo) GUINT_TO_POINTER(0x80000000 | (lpo)->ack_needed)

/* NOTE: this must not evaluate ptr multiple times, otherwise the code that
 * uses this breaks, as it passes the result of a g_queue_pop_head call,
 * which has side effects.
 */
#define POINTER_TO_LOG_PATH_OPTIONS(ptr, lpo) \
({ \
  gpointer p = ptr; \
  (lpo)->ack_needed = (GPOINTER_TO_INT(p) & ~0x80000000); \
})

typedef gboolean (*QDiskSerializeFunc)(SerializeArchive *sa, gpointer user_data);
typedef gboolean (*QDiskDeSerializeFunc)(SerializeArchive *sa, gpointer user_data);

typedef struct
{
  gint64 ofs;
  guint32 len;
  guint32 count;
}
QDiskQueuePosition;

typedef struct _QDisk QDisk;

QDisk *qdisk_new(DiskQueueOptions *options, const gchar *file_id, const gchar *filename);

gboolean qdisk_is_space_avail(QDisk *self, gint at_least);
gint64 qdisk_get_max_useful_space(QDisk *self);
gint64 qdisk_get_empty_space(QDisk *self);
gint64 qdisk_get_used_useful_space(QDisk *self);
gboolean qdisk_push_tail(QDisk *self, GString *record);
gboolean qdisk_pop_head(QDisk *self, GString *record);
gboolean qdisk_peek_head(QDisk *self, GString *record);
gboolean qdisk_remove_head(QDisk *self);
gboolean qdisk_ack_backlog(QDisk *self);
gboolean qdisk_rewind_backlog(QDisk *self, guint rewind_count);
void qdisk_empty_backlog(QDisk *self);
gint64 qdisk_get_next_tail_position(QDisk *self);
gint64 qdisk_get_next_head_position(QDisk *self);
gboolean qdisk_start(QDisk *self, GQueue *front_cache, GQueue *backlog, GQueue *flow_control_window);
gboolean qdisk_stop(QDisk *self, GQueue *front_cache, GQueue *backlog, GQueue *flow_control_window);
void qdisk_reset_file_if_empty(QDisk *self);
gboolean qdisk_started(QDisk *self);
void qdisk_free(QDisk *self);

DiskQueueOptions *qdisk_get_options(QDisk *self);
gint64 qdisk_get_length(QDisk *self);
gint64 qdisk_get_maximum_size(QDisk *self);
gint64 qdisk_get_writer_head(QDisk *self);
gint64 qdisk_get_reader_head(QDisk *self);
gint64 qdisk_get_backlog_head(QDisk *self);
gint64 qdisk_get_backlog_count(QDisk *self);
gint qdisk_get_flow_control_window_bytes(QDisk *self);
gboolean qdisk_is_read_only(QDisk *self);
const gchar *qdisk_get_filename(QDisk *self);
gint64 qdisk_get_file_size(QDisk *self);

gchar *qdisk_get_next_filename(const gchar *dir, gboolean reliable);
gboolean qdisk_is_file_a_disk_buffer_file(const gchar *filename);
gboolean qdisk_is_disk_buffer_file_reliable(const gchar *filename, gboolean *reliable);

gboolean qdisk_serialize(GString *serialized, QDiskSerializeFunc serialize_func, gpointer user_data, GError **error);
gboolean qdisk_deserialize(GString *serialized, QDiskDeSerializeFunc deserialize_func, gpointer user_data,
                           GError **error);

#endif /* QDISK_H_ */
