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
#define POINTER_TO_LOG_PATH_OPTIONS(ptr, lpo) (lpo)->ack_needed = (GPOINTER_TO_INT(ptr) & ~0x80000000)

typedef struct _QDisk QDisk;

QDisk *qdisk_new(void);

gboolean qdisk_is_space_avail(QDisk *self, gint at_least);
gint64 qdisk_get_empty_space(QDisk *self);
gboolean qdisk_push_tail(QDisk *self, GString *record);
gboolean qdisk_pop_head(QDisk *self, GString *record);
gboolean qdisk_start(QDisk *self, const gchar *filename, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow);
void qdisk_init(QDisk *self, DiskQueueOptions *options);
void qdisk_deinit(QDisk *self);
void qdisk_reset_file_if_possible(QDisk *self);
gboolean qdisk_initialized(QDisk *self);
void qdisk_free(QDisk *self);

gboolean qdisk_save_state(QDisk *self, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow);

DiskQueueOptions *qdisk_get_options(QDisk *self);
gint64 qdisk_get_length(QDisk *self);
void qdisk_set_length(QDisk *self, gint64 new_value);
gint64 qdisk_get_size(QDisk *self);
gint64 qdisk_get_writer_head(QDisk *self);
gint64 qdisk_get_reader_head(QDisk *self);
void qdisk_set_reader_head(QDisk *self, gint64 new_value);
gint64 qdisk_get_backlog_head(QDisk *self);
void qdisk_set_backlog_head(QDisk *self, gint64 new_value);
void qdisk_inc_backlog(QDisk *self);
void qdisk_dec_backlog(QDisk *self);
gint64 qdisk_get_backlog_count(QDisk *self);
void qdisk_set_backlog_count(QDisk *self, gint64 new_value);
gint qdisk_get_memory_size(QDisk *self);
gboolean qdisk_is_read_only(QDisk *self);
const gchar *qdisk_get_filename(QDisk *self);

gssize qdisk_read_from_backlog(QDisk *self, gpointer buffer, gsize bytes_to_read);
gssize qdisk_read(QDisk *self, gpointer buffer, gsize bytes_to_read, gint64 position);
guint64 qdisk_skip_record(QDisk *self, guint64 position);

#endif /* QDISK_H_ */
