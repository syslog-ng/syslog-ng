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

#include "qdisk.h"
#include "logpipe.h"
#include "messages.h"
#include "serialize.h"
#include "logmsg/logmsg-serialize.h"
#include "stats/stats-registry.h"
#include "reloc.h"
#include "compat/lfs.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

/* MADV_RANDOM not defined on legacy Linux systems. Could be removed in the
 * future, when support for Glibc 2.1.X drops.*/
#ifndef MADV_RANDOM
#define MADV_RANDOM 1
#endif

#define MAX_RECORD_LENGTH 100 * 1024 * 1024

#define PATH_QDISK              PATH_LOCALSTATEDIR

typedef union _QDiskFileHeader
{
  struct
  {
    gchar magic[4];
    guint8 version;
    guint8 big_endian;
    guint8 _pad1;

    gint64 read_head;
    gint64 write_head;
    gint64 length;

    QDiskQueuePosition qout_pos;
    QDiskQueuePosition qbacklog_pos;
    QDiskQueuePosition qoverflow_pos;
    gint64 backlog_head;
    gint64 backlog_len;
  };
  gchar _pad2[QDISK_RESERVED_SPACE];
} QDiskFileHeader;

struct _QDisk
{
  gchar *filename;
  const gchar *file_id;
  gint fd;
  gint64 file_size;
  QDiskFileHeader *hdr;
  DiskQueueOptions *options;
};

static gboolean
pwrite_strict(gint fd, const void *buf, size_t count, off_t offset)
{
  ssize_t written = pwrite(fd, buf, count, offset);
  gboolean result = TRUE;
  if (written != count)
    {
      if (written != -1)
        {
          msg_error("Short write while writing disk buffer",
                    evt_tag_int("bytes_to_write", count),
                    evt_tag_int("bytes_written", written));
          errno = ENOSPC;
        }
      result = FALSE;
    }
  return result;
}


static gboolean
_is_position_eof(QDisk *self, gint64 position)
{
  return position >= self->file_size;
}

static guint64
_correct_position_if_eof(QDisk *self, gint64 *position)
{
  if (_is_position_eof(self, *position))
    {
      *position = QDISK_RESERVED_SPACE;
    }
  return *position;
}

static gchar *
_next_filename(QDisk *self)
{
  gint i = 0;
  gboolean success = FALSE;
  gchar tmpfname[256];
  gchar qdir[256];

  g_snprintf(qdir, sizeof(qdir), "%s", self->options->dir);

  /* NOTE: this'd be a security problem if we were not in our private directory. But we are. */
  while (!success && i < 100000)
    {
      struct stat st;

      const gchar *format = "%s/syslog-ng-%05d.qf";
      if (self->options->reliable)
        format = "%s/syslog-ng-%05d.rqf";
      g_snprintf(tmpfname, sizeof(tmpfname), format, qdir, i);
      success = (stat(tmpfname, &st) < 0);
      i++;
    }
  if (!success)
    {
      msg_error("Error generating unique queue filename, not using disk queue");
      return NULL;
    }
  return g_strdup(tmpfname);
}

gboolean
qdisk_started(QDisk *self)
{
  return self->fd >= 0;
}

static inline gboolean
_is_backlog_head_prevent_write_head(QDisk *self)
{
  return self->hdr->backlog_head <= self->hdr->write_head;
}

static inline gboolean
_is_write_head_less_than_max_size(QDisk *self)
{
  return self->hdr->write_head < self->options->disk_buf_size;
}

static inline gboolean
_is_able_to_reset_write_head_to_beginning_of_qdisk(QDisk *self)
{
  return self->hdr->backlog_head != QDISK_RESERVED_SPACE;
}

static inline gboolean
_is_free_space_between_write_head_and_backlog_head(QDisk *self, gint msg_len)
{
  return self->hdr->write_head + msg_len < self->hdr->backlog_head;
}

gboolean
qdisk_is_file_empty(QDisk *self)
{
  return self->hdr->length == 0 && self->hdr->backlog_len == 0;
}

gboolean
qdisk_is_space_avail(QDisk *self, gint at_least)
{
  gint64 msg_len = at_least + sizeof(guint32);
  return (
           (_is_backlog_head_prevent_write_head(self)) &&
           (_is_write_head_less_than_max_size(self) || _is_able_to_reset_write_head_to_beginning_of_qdisk(self))
         ) || (_is_free_space_between_write_head_and_backlog_head(self, msg_len));

}

static gboolean
_truncate_file(QDisk *self, gint64 new_size)
{
  gboolean success = TRUE;

  if (ftruncate(self->fd, (glong)new_size) < 0)
    {
      success = FALSE;
      off_t file_size = -1;

      struct stat st;
      if (fstat(self->fd, &st) < 0)
        {
          msg_error("truncate file: cannot stat",
                    evt_tag_error("error"));
        }
      else
        {
          file_size = st.st_size;
        }

      msg_error("Error truncating disk-queue file",
                evt_tag_error("error"),
                evt_tag_str("filename", self->filename),
                evt_tag_long("expected-size", new_size),
                evt_tag_long("file_size", file_size),
                evt_tag_int("fd", self->fd));
    }

  return success;
}

static gint64
qdisk_get_lowest_used_queue_offset(QDisk *self)
{
  gint64 lowest_offset = G_MAXINT64;

  if (self->hdr->qout_pos.ofs > 0)
    lowest_offset = MIN(lowest_offset, self->hdr->qout_pos.ofs);

  if (self->hdr->qbacklog_pos.ofs > 0)
    lowest_offset = MIN(lowest_offset, self->hdr->qbacklog_pos.ofs);

  if (self->hdr->qoverflow_pos.ofs > 0)
    lowest_offset = MIN(lowest_offset, self->hdr->qoverflow_pos.ofs);

  return lowest_offset < G_MAXINT64 ? lowest_offset : 0;
}

static void
qdisk_try_to_truncate_file_to_minimal(QDisk *self, gint64 *new_file_end_offset)
{
  gint64 file_end_offset = 0;
  if (qdisk_is_file_empty(self))
    {
      _truncate_file(self, QDISK_RESERVED_SPACE);
      file_end_offset = QDISK_RESERVED_SPACE;
    }
  else
    {
      file_end_offset = qdisk_get_lowest_used_queue_offset(self);
      if(file_end_offset > QDISK_RESERVED_SPACE)
        _truncate_file(self, file_end_offset);
    }
  if (new_file_end_offset)
    *new_file_end_offset = file_end_offset;
}


gint64
qdisk_get_empty_space(QDisk *self)
{
  gint64 wpos = qdisk_get_writer_head(self);
  gint64 bpos = qdisk_get_backlog_head(self);

  if (wpos > bpos)
    {
      return (qdisk_get_size(self) - wpos) +
             (bpos - QDISK_RESERVED_SPACE);
    }

  return bpos - wpos;
}

gboolean
qdisk_push_tail(QDisk *self, GString *record)
{
  guint32 n = GUINT32_TO_BE(record->len);

  /* write follows read (e.g. we are appending to the file) OR
   * there's enough space between write and read.
   *
   * If write follows read we need to check two things:
   *   - either we are below the maximum limit (GINT64_FROM_BE(self->hdr->write_head) < self->options->disk_buf_size)
   *   - or we can wrap around (GINT64_FROM_BE(self->hdr->read_head) != QDISK_RESERVED_SPACE)
   * If neither of the above is true, the buffer is full.
   */
  if (!qdisk_is_space_avail(self, record->len))
    return FALSE;

  if (n == 0)
    {
      msg_error("Error writing empty message into the disk-queue file");
      return FALSE;
    }

  if (!pwrite_strict(self->fd, (gchar *) &n, sizeof(n), self->hdr->write_head) ||
      !pwrite_strict(self->fd, record->str, record->len, self->hdr->write_head + sizeof(n)))
    {
      msg_error("Error writing disk-queue file",
                evt_tag_error("error"));
      return FALSE;
    }

  self->hdr->write_head = self->hdr->write_head + record->len + sizeof(n);


  /* NOTE: we only wrap around if the read head is before the write,
   * otherwise we'd truncate the data the read head is still processing, e.g.
   *
   * start                limit       file size
   *   |                     |           |
   *       ^ read         ^write
   *
   * We can truncate in the scenario above, once we surpass the limit,
   * however if this is the case:
   *
   * start                limit       file size
   *   |                     |           |
   *                      ^write    ^read
   *
   * In this case cannot wrap around, even if the limit is surpassed, as
   * in that case data still unprocessed by the read head would be lost.
   * This can happen if the size of the queue file got decreased while
   * it still had more data.
   * */

  /* NOTE: if these were equal, that'd mean the queue is empty, so we spoiled something */
  g_assert(self->hdr->write_head != self->hdr->backlog_head);

  if (self->hdr->write_head > MAX(self->hdr->backlog_head,self->hdr->read_head))
    {
      if (self->file_size > self->hdr->write_head)
        {
          _truncate_file(self, self->hdr->write_head);
        }
      self->file_size = self->hdr->write_head;

      if (self->hdr->write_head > self->options->disk_buf_size && self->hdr->backlog_head  != QDISK_RESERVED_SPACE)
        {
          /* we were appending to the file, we are over the limit, and space
           * is available before the read head. truncate and wrap.
           *
           * Otherwise we let the write_head over size limits for a bit and
           * for the next message, the condition at the beginning of this
           * function will cause the push to fail */
          self->hdr->write_head = QDISK_RESERVED_SPACE;
        }
    }
  self->hdr->length++;
  return TRUE;
}

static inline gboolean
_is_record_length_reached_hard_limit(guint32 record_length)
{
  return record_length > MAX_RECORD_LENGTH;
}

gboolean
qdisk_pop_head(QDisk *self, GString *record)
{
  if (self->hdr->read_head != self->hdr->write_head)
    {
      guint32 n;
      gssize res;
      res = pread(self->fd, (gchar *) &n, sizeof(n), self->hdr->read_head);

      if (res == 0)
        {
          /* hmm, we are either at EOF or at hdr->qout_ofs, we need to wrap */
          self->hdr->read_head = QDISK_RESERVED_SPACE;
          res = pread(self->fd, (gchar *) &n, sizeof(n), self->hdr->read_head);
        }
      if (res != sizeof(n))
        {
          msg_error("Error reading disk-queue file",
                    evt_tag_str("error", res < 0 ? g_strerror(errno) : "short read"),
                    evt_tag_str("filename", self->filename));
          return FALSE;
        }

      n = GUINT32_FROM_BE(n);
      if (_is_record_length_reached_hard_limit(n))
        {
          msg_warning("Disk-queue file contains possibly invalid record-length",
                      evt_tag_int("rec_length", n),
                      evt_tag_str("filename", self->filename));
          return FALSE;
        }
      else if (n == 0)
        {
          msg_error("Disk-queue file contains empty record",
                    evt_tag_int("rec_length", n),
                    evt_tag_str("filename", self->filename));
          return FALSE;
        }

      g_string_set_size(record, n);
      res = pread(self->fd, record->str, n, self->hdr->read_head + sizeof(n));
      if (res != n)
        {
          msg_error("Error reading disk-queue file",
                    evt_tag_str("filename", self->filename),
                    evt_tag_str("error", res < 0 ? g_strerror(errno) : "short read"),
                    evt_tag_int("read_length", n));
          return FALSE;
        }

      self->hdr->read_head = self->hdr->read_head + record->len + sizeof(n);

      if (self->hdr->read_head > self->hdr->write_head)
        {
          self->hdr->read_head = _correct_position_if_eof(self, &self->hdr->read_head);
        }

      self->hdr->length--;
      if (!self->options->reliable)
        {
          self->hdr->backlog_head = self->hdr->read_head;
        }

      if (!self->options->read_only && self->hdr->length == 0 && !self->options->reliable)
        {
          msg_debug("Queue file became empty, truncating file",
                    evt_tag_str("filename", self->filename));
          self->hdr->read_head = QDISK_RESERVED_SPACE;
          self->hdr->write_head = QDISK_RESERVED_SPACE;
          if (!self->options->reliable)
            {
              self->hdr->backlog_head = self->hdr->read_head;
            }
          self->hdr->length = 0;
          _truncate_file(self, self->hdr->write_head);
        }
      return TRUE;

    }
  return FALSE;
}

static gboolean
_load_queue(QDisk *self, GQueue *q, gint64 q_ofs, gint32 q_len, gint32 q_count)
{
  GString *serialized;
  SerializeArchive *sa;
  gint i;

  if (q_ofs)
    {
      gssize read_len;

      serialized = g_string_sized_new(q_len);
      g_string_set_size(serialized, q_len);
      read_len = pread(self->fd, serialized->str, q_len, q_ofs);
      if (read_len < 0 || read_len != q_len)
        {
          msg_error("Error reading in-memory buffer from disk-queue file",
                    evt_tag_str("filename", self->filename),
                    read_len < 0 ? evt_tag_errno("error", errno) : evt_tag_str("error", "short read"));
          g_string_free(serialized, TRUE);
          return FALSE;
        }
      sa = serialize_string_archive_new(serialized);
      for (i = 0; i < q_count; i++)
        {
          LogMessage *msg;

          msg = log_msg_new_empty();
          if (log_msg_deserialize(msg, sa))
            {
              g_queue_push_tail(q, msg);
              /* we restore the queue without ACKs */
              g_queue_push_tail(q, LOG_PATH_OPTIONS_FOR_BACKLOG);
            }
          else
            {
              msg_error("Error reading message from disk-queue file (maybe corrupted file) some messages will be lost",
                        evt_tag_str("filename", self->filename),
                        evt_tag_int("lost_messages", q_count - i));
              log_msg_unref(msg);
              break;
            }
        }
      g_string_free(serialized, TRUE);
      serialize_archive_free(sa);
    }
  return TRUE;
}

static gboolean
_try_to_load_queue(QDisk *self, GQueue *queue, QDiskQueuePosition *pos, gchar *type)
{
  gint64 ofs;
  gint32 count, len;

  count = pos->count;
  len = pos->len;
  ofs = pos->ofs;

  if (!(ofs > 0 && ofs < self->hdr->write_head))
    {
      if (!_load_queue(self, queue, ofs, len, count))
        return !self->options->read_only;
    }
  else
    {
      msg_error("Inconsistent header data in disk-queue file, ignoring queue",
                evt_tag_str("filename", self->filename),
                evt_tag_long("write_head", self->hdr->write_head),
                evt_tag_str("type", type),
                evt_tag_long("ofs", ofs),
                evt_tag_long("qdisk_length",  self->hdr->length));
    }

  return TRUE;
}

#define try_load_queue(self, queue) _try_to_load_queue(self, queue, &self->hdr->queue ##_pos, #queue)

static gboolean
_load_non_reliable_queues(QDisk *self, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  if (!try_load_queue(self, qout))
    return FALSE;
  if (!try_load_queue(self, qbacklog))
    return FALSE;
  if (!try_load_queue(self, qoverflow))
    return FALSE;

  return TRUE;
}

#define _clear(obj) memset(&obj, 0, sizeof(obj));

static void
_reset_queue_pointers(QDisk *self)
{
  _clear(self->hdr->qout_pos);
  _clear(self->hdr->qbacklog_pos);
  _clear(self->hdr->qoverflow_pos);
};

static gboolean
qdisk_header_is_inconsistent(QDisk *self)
{
  return ((self->hdr->read_head < QDISK_RESERVED_SPACE) ||
          (self->hdr->write_head < QDISK_RESERVED_SPACE) ||
          (self->hdr->read_head == self->hdr->write_head && self->hdr->length != 0));
}


static gboolean
_load_state(QDisk *self, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  gint64 qout_ofs;
  gint qout_count;
  gint64 qbacklog_ofs;
  gint qbacklog_count;
  gint64 qoverflow_ofs;
  gint qoverflow_count;
  gint64 end_ofs = QDISK_RESERVED_SPACE;

  if (memcmp(self->hdr->magic, self->file_id, 4) != 0)
    {
      msg_error("Error reading disk-queue file header",
                evt_tag_str("filename", self->filename));
      return FALSE;
    }

  qout_count = self->hdr->qout_pos.count;
  qout_ofs = self->hdr->qout_pos.ofs;
  qbacklog_count = self->hdr->qbacklog_pos.count;
  qbacklog_ofs = self->hdr->qbacklog_pos.ofs;
  qoverflow_count = self->hdr->qoverflow_pos.count;
  qoverflow_ofs = self->hdr->qoverflow_pos.ofs;

  if (qdisk_header_is_inconsistent(self))
    {
      msg_error("Inconsistent header data in disk-queue file, ignoring",
                evt_tag_str("filename", self->filename),
                evt_tag_long("read_head", self->hdr->read_head),
                evt_tag_long("write_head", self->hdr->write_head),
                evt_tag_long("qdisk_length",  self->hdr->length));
      return FALSE;
    }

  if (!self->options->reliable)
    {
      if (!_load_non_reliable_queues(self, qout, qbacklog, qoverflow))
        return FALSE;

      if (!self->options->read_only)
        {
          qdisk_try_to_truncate_file_to_minimal(self, &end_ofs);
        }

  if (!self->options->reliable)
    {
      self->file_size = MAX(end_ofs, QDISK_RESERVED_SPACE);
      _reset_queue_pointers(self);

      msg_info("Disk-buffer state loaded",
               evt_tag_str("filename", self->filename),
               evt_tag_int("qout_length", qout_count),
               evt_tag_int("qbacklog_length", qbacklog_count),
               evt_tag_int("qoverflow_length", qoverflow_count),
               evt_tag_long("qdisk_length", self->hdr->length));
    }
  else
    {
      struct stat st;
      fstat(self->fd, &st);
      self->file_size = st.st_size;
      msg_info("Reliable disk-buffer state loaded",
               evt_tag_str("filename", self->filename),
               evt_tag_long("queue_length", self->hdr->length),
               evt_tag_long("size", self->hdr->write_head - self->hdr->read_head));

      msg_debug("Reliable disk-buffer internal state",
                evt_tag_str("filename", self->filename),
                evt_tag_long("backlog_head", self->hdr->backlog_head),
                evt_tag_long("read_head", self->hdr->read_head),
                evt_tag_long("write_head", self->hdr->write_head),
                evt_tag_long("backlog_len", self->hdr->backlog_len));
    }

  return TRUE;
}

#define STRING_BUFFER_MEMORY_LIMIT (8 * 1024)
static inline gboolean
string_reached_memory_limit(GString *string)
{
  return (string->len >= STRING_BUFFER_MEMORY_LIMIT);
}

static gboolean
qdisk_write_serialized_string_to_file(QDisk *self, GString const *serialized, gint64 *offset)
{
  *offset = lseek(self->fd, 0, SEEK_END);
  if (!pwrite_strict(self->fd, serialized->str, serialized->len, *offset))
    {
      msg_error("Error writing in-memory buffer of disk-queue to disk",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_save_queue(QDisk *self, GQueue *q, gint64 *q_ofs, gint32 *q_len)
{
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  SerializeArchive *sa;
  GString *serialized;
  gint64  current_offset = 0;
  gint64  queue_start_position = 0;
  gint32  written_bytes = 0;
  gboolean success = FALSE;

  if (q->length == 0)
    {
      *q_ofs = 0;
      *q_len = 0;
      return TRUE;
    }

  serialized = g_string_sized_new(4096);
  sa = serialize_string_archive_new(serialized);
  while ((msg = g_queue_pop_head(q)))
    {

      /* NOTE: we might have some flow-controlled events on qout, when
       * saving them to disk, we ack them, they are restored as
       * non-flow-controlled entries later, but then we've saved them to
       * disk anyway. */

      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(q), &path_options);
      log_msg_serialize(msg, sa);
      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
      if (string_reached_memory_limit(serialized))
        {
          if (!qdisk_write_serialized_string_to_file(self, serialized, &current_offset))
            goto error;
          if(!queue_start_position)
            queue_start_position = current_offset;
          written_bytes += serialized->len;
          g_string_truncate(serialized, 0);
        }
    }
  if(serialized->len)
    {
      if (!qdisk_write_serialized_string_to_file(self, serialized, &current_offset))
        goto error;
      if(!queue_start_position)
        queue_start_position = current_offset;
      written_bytes += serialized->len;
    }

  *q_len = written_bytes;
  *q_ofs = queue_start_position;
  success = TRUE;
error:
  g_string_free(serialized, TRUE);
  serialize_archive_free(sa);
  return success;
}

gboolean
qdisk_save_state(QDisk *self, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  gint64 qout_ofs = 0;
  gint32 qout_len = 0;
  gint32 qout_count = 0;
  gint64 qbacklog_ofs = 0;
  gint32 qbacklog_len = 0;
  gint32 qbacklog_count = 0;
  gint64 qoverflow_ofs = 0;
  gint32 qoverflow_len = 0;
  gint32 qoverflow_count = 0;

  if (!self->options->reliable)
    {
      qout_count = qout->length / 2;
      qbacklog_count = qbacklog->length / 2;
      qoverflow_count = qoverflow->length / 2;

      if (!_save_queue(self, qout, &qout_ofs, &qout_len) ||
          !_save_queue(self, qbacklog, &qbacklog_ofs, &qbacklog_len) ||
          !_save_queue(self, qoverflow, &qoverflow_ofs, &qoverflow_len))
        return FALSE;
    }

  memcpy(self->hdr->magic, self->file_id, 4);

  self->hdr->qout_pos.ofs = qout_ofs;
  self->hdr->qout_pos.len = qout_len;
  self->hdr->qout_pos.count = qout_count;

  self->hdr->qbacklog_pos.ofs = qbacklog_ofs;
  self->hdr->qbacklog_pos.len = qbacklog_len;
  self->hdr->qbacklog_pos.count = qbacklog_count;

  self->hdr->qoverflow_pos.ofs = qoverflow_ofs;
  self->hdr->qoverflow_pos.len = qoverflow_len;
  self->hdr->qoverflow_pos.count = qoverflow_count;

  if (!self->options->reliable)
    msg_info("Disk-buffer state saved",
             evt_tag_str("filename", self->filename),
             evt_tag_int("qout_length", qout_count),
             evt_tag_int("qbacklog_length", qbacklog_count),
             evt_tag_int("qoverflow_length", qoverflow_count),
             evt_tag_long("qdisk_length", self->hdr->length));
  else
    msg_info("Reliable disk-buffer state saved",
             evt_tag_str("filename", self->filename),
             evt_tag_long("qdisk_length", self->hdr->length));

  return TRUE;
}

static void
_update_header_with_default_values(QDisk *self)
{
  self->hdr->big_endian = TRUE;
  self->hdr->version = 1;
  self->hdr->backlog_head = self->hdr->read_head;
  self->hdr->backlog_len = 0;
}

gboolean
qdisk_start(QDisk *self, const gchar *filename, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  gboolean new_file = FALSE;
  gpointer p = NULL;
  int openflags = 0;

  /*
   * If qdisk_start is called for already initialized qdisk file
   * it can cause message loosing.
   * We need this assert to detect programming error as soon as possible.
   */
  g_assert(!qdisk_started(self));

  if (self->options->disk_buf_size <= 0)
    return TRUE;

  if (self->options->read_only && !filename)
    return FALSE;

  if (!filename)
    {
      new_file = TRUE;
      /* NOTE: this'd be a security problem if we were not in our private directory. But we are. */
      filename = _next_filename(self);
    }
  else
    {
      struct stat st;
      if (stat(filename,&st) == -1)
        {
          new_file = TRUE;
        }
    }

  self->filename = g_strdup(filename);
  /* assumes self is zero initialized */
  openflags = self->options->read_only ? (O_RDONLY | O_LARGEFILE) : (O_RDWR | O_LARGEFILE | (new_file ? O_CREAT : 0));

  self->fd = open(filename, openflags, 0600);
  if (self->fd < 0)
    {
      msg_error("Error opening disk-queue file",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));
      return FALSE;
    }

  p = mmap(0, sizeof(QDiskFileHeader),  self->options->read_only ? (PROT_READ) : (PROT_READ | PROT_WRITE), MAP_SHARED,
           self->fd, 0);

  if (p == MAP_FAILED)
    {
      msg_error("Error returned by mmap",
                evt_tag_error("errno"),
                evt_tag_str("filename", self->filename));
      return FALSE;
    }
  else
    {
      madvise(p, sizeof(QDiskFileHeader), MADV_RANDOM);
    }

  if (self->options->read_only)
    {
      self->hdr = g_malloc(sizeof(QDiskFileHeader));
      memcpy(self->hdr, p, sizeof(QDiskFileHeader));
      munmap(p, sizeof(QDiskFileHeader) );
      p = NULL;
    }
  else
    {
      self->hdr = p;
    }
  /* initialize new file */

  if (new_file)
    {
      QDiskFileHeader tmp;
      memset(&tmp, 0, sizeof(tmp));
      if (!pwrite_strict(self->fd, &tmp, sizeof(tmp), 0))
        {
          msg_error("Error occurred while initalizing the new queue file",
                    evt_tag_str("filename", self->filename),
                    evt_tag_error("error"));
          munmap((void *)self->hdr, sizeof(QDiskFileHeader));
          self->hdr = NULL;
          close(self->fd);
          self->fd = -1;
          return FALSE;
        }
      self->hdr->version = 1;
      self->hdr->big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);

      self->hdr->read_head = QDISK_RESERVED_SPACE;
      self->hdr->write_head = QDISK_RESERVED_SPACE;
      self->hdr->backlog_head = self->hdr->read_head;
      self->hdr->length = 0;
      self->file_size = self->hdr->write_head;

      if (!qdisk_save_state(self, qout, qbacklog, qoverflow))
        {
          munmap((void *)self->hdr, sizeof(QDiskFileHeader));
          self->hdr = NULL;
          close(self->fd);
          self->fd = -1;
          return FALSE;
        }
    }
  else
    {
      struct stat st;

      if (fstat(self->fd, &st) != 0 || st.st_size == 0)
        {
          msg_error("Error loading disk-queue file",
                    evt_tag_str("filename", self->filename),
                    evt_tag_error("fstat error"),
                    evt_tag_int("size", st.st_size));
          munmap((void *)self->hdr, sizeof(QDiskFileHeader));
          self->hdr = NULL;
          close(self->fd);
          self->fd = -1;
          return FALSE;
        }
      if (self->hdr->version == 0)
        _update_header_with_default_values(self);

      if ((self->hdr->big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
          (!self->hdr->big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
        {
          self->hdr->read_head = GUINT64_SWAP_LE_BE(self->hdr->read_head);
          self->hdr->write_head = GUINT64_SWAP_LE_BE(self->hdr->write_head);
          self->hdr->length = GUINT64_SWAP_LE_BE(self->hdr->length);
          self->hdr->qout_pos.ofs = GUINT64_SWAP_LE_BE(self->hdr->qout_pos.ofs);
          self->hdr->qout_pos.len = GUINT32_SWAP_LE_BE(self->hdr->qout_pos.len);
          self->hdr->qout_pos.count = GUINT32_SWAP_LE_BE(self->hdr->qout_pos.count);
          self->hdr->qbacklog_pos.ofs = GUINT64_SWAP_LE_BE(self->hdr->qbacklog_pos.ofs);
          self->hdr->qbacklog_pos.len = GUINT32_SWAP_LE_BE(self->hdr->qbacklog_pos.len);
          self->hdr->qbacklog_pos.count = GUINT32_SWAP_LE_BE(self->hdr->qbacklog_pos.count);
          self->hdr->qoverflow_pos.ofs = GUINT64_SWAP_LE_BE(self->hdr->qoverflow_pos.ofs);
          self->hdr->qoverflow_pos.len = GUINT32_SWAP_LE_BE(self->hdr->qoverflow_pos.len);
          self->hdr->qoverflow_pos.count = GUINT32_SWAP_LE_BE(self->hdr->qoverflow_pos.count);
          self->hdr->backlog_head = GUINT64_SWAP_LE_BE(self->hdr->backlog_head);
          self->hdr->backlog_len = GUINT64_SWAP_LE_BE(self->hdr->backlog_len);
          self->hdr->big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
        }
      if (!_load_state(self, qout, qbacklog, qoverflow))
        {
          munmap((void *)self->hdr, sizeof(QDiskFileHeader));
          self->hdr = NULL;
          close(self->fd);
          self->fd = -1;
          return FALSE;
        }

    }
  return TRUE;
}

void
qdisk_init_instance(QDisk *self, DiskQueueOptions *options, const gchar *file_id)
{
  self->fd = -1;
  self->file_size = 0;
  self->options = options;

  self->file_id = file_id;
}

void
qdisk_stop(QDisk *self)
{
  if (self->filename)
    {
      g_free(self->filename);
      self->filename = NULL;
    }

  if (self->hdr)
    {
      if (self->options->read_only)
        g_free(self->hdr);
      else
        munmap((void *)self->hdr, sizeof(QDiskFileHeader));
      self->hdr = NULL;
    }

  if (self->fd != -1)
    {
      close(self->fd);
      self->fd = -1;
    }

  self->options = NULL;
}

gssize
qdisk_read(QDisk *self, gpointer buffer, gsize bytes_to_read, gint64 position)
{
  gssize res;
  res = pread(self->fd, buffer, bytes_to_read, position);
  if (res <= 0)
    {
      msg_error("Error reading disk-queue file",
                evt_tag_str("error", res < 0 ? g_strerror(errno) : "short read"),
                evt_tag_str("filename", self->filename));
    }
  return res;
}

guint64
qdisk_skip_record(QDisk *self, guint64 position)
{
  guint64 new_position = position;
  guint32 s;
  qdisk_read (self, (gchar *) &s, sizeof(s), position);
  s = GUINT32_FROM_BE(s);
  new_position += s + sizeof(s);
  if (new_position > self->hdr->write_head)
    {
      new_position = _correct_position_if_eof(self, (gint64 *)&new_position);
    }
  return new_position;
}

void
qdisk_set_backlog_count(QDisk *self, gint64 new_value)
{
  self->hdr->backlog_len = new_value;
}

void
qdisk_reset_file_if_possible(QDisk *self)
{
  if (qdisk_is_file_empty(self))
    {
      self->hdr->read_head = QDISK_RESERVED_SPACE;
      self->hdr->write_head = QDISK_RESERVED_SPACE;
      self->hdr->backlog_head = QDISK_RESERVED_SPACE;
      _truncate_file (self, QDISK_RESERVED_SPACE);
    }
}

DiskQueueOptions *
qdisk_get_options(QDisk *self)
{
  return self->options;
}

gint64
qdisk_get_length(QDisk *self)
{
  return self->hdr->length;
}

void
qdisk_set_length(QDisk *self, gint64 new_value)
{
  self->hdr->length = new_value;
}

gint64
qdisk_get_size(QDisk *self)
{
  return self->options->disk_buf_size;
}

const gchar *
qdisk_get_filename(QDisk *self)
{
  return self->filename;
}

gint64
qdisk_get_writer_head(QDisk *self)
{
  return self->hdr->write_head;
}

gint64
qdisk_get_reader_head(QDisk *self)
{
  return self->hdr->read_head;
}

void
qdisk_set_reader_head(QDisk *self, gint64 new_value)
{
  self->hdr->read_head = new_value;
}

gint64
qdisk_get_backlog_head(QDisk *self)
{
  return self->hdr->backlog_head;
}

void
qdisk_set_backlog_head(QDisk *self, gint64 new_value)
{
  self->hdr->backlog_head = new_value;
}

void
qdisk_inc_backlog(QDisk *self)
{
  self->hdr->backlog_len++;
}

void
qdisk_dec_backlog(QDisk *self)
{
  self->hdr->backlog_len--;
}

gint64
qdisk_get_backlog_count(QDisk *self)
{
  return self->hdr->backlog_len;
}

gint
qdisk_get_memory_size(QDisk *self)
{
  return self->options->mem_buf_size;
}

gboolean
qdisk_is_read_only(QDisk *self)
{
  return self->options->read_only;
}

void
qdisk_free(QDisk *self)
{
  g_free(self);
}

QDisk *
qdisk_new(void)
{
  QDisk *self = g_new0(QDisk, 1);
  return self;
}
