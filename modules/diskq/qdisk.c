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
#include "scratch-buffers.h"

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

#define QDISK_HDR_VERSION_CURRENT 3

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

    guint8 use_v1_wrap_condition;
    gint64 disk_buf_size;
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

#define QDISK_ERROR qdisk_error_quark()
#define QDISK_ERROR_SERIALIZE 0
#define QDISK_ERROR_DESERIALIZE 1

GQuark
qdisk_error_quark(void)
{
  static GQuark q = 0;

  if (q == 0)
    q = g_quark_from_static_string("qdisk-error");

  return q;
}

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


static inline gboolean
_has_position_reached_max_size(QDisk *self, gint64 position)
{
  return position >= self->hdr->disk_buf_size;
}

static inline guint64
_correct_position_if_max_size_is_reached(QDisk *self, gint64 position)
{
  if (G_UNLIKELY(self->hdr->use_v1_wrap_condition))
    {
      gboolean position_is_eof = position >= self->file_size;
      if (position_is_eof)
        {
          position = QDISK_RESERVED_SPACE;
          self->hdr->use_v1_wrap_condition = FALSE;
        }
      return position;
    }

  if (_has_position_reached_max_size(self, position))
    {
      position = QDISK_RESERVED_SPACE;
    }
  return position;
}

static gboolean
_next_filename(QDisk *self, gchar *filename, gsize filename_len)
{
  gint i = 0;
  gboolean success = FALSE;
  gchar qdir[256];

  g_snprintf(qdir, sizeof(qdir), "%s", self->options->dir);

  /* NOTE: this'd be a security problem if we were not in our private directory. But we are. */
  while (!success && i < 100000)
    {
      struct stat st;

      if (self->options->reliable)
        g_snprintf(filename, filename_len, "%s/syslog-ng-%05d.rqf", qdir, i);
      else
        g_snprintf(filename, filename_len, "%s/syslog-ng-%05d.qf", qdir, i);
      success = (stat(filename, &st) < 0);
      i++;
    }

  if (!success)
    {
      msg_error("Error generating unique queue filename, not using disk queue");
      return FALSE;
    }

  return TRUE;
}

gboolean
qdisk_started(QDisk *self)
{
  return self->fd >= 0;
}

static inline gboolean
_does_backlog_head_precede_write_head(QDisk *self)
{
  return self->hdr->backlog_head <= self->hdr->write_head;
}

static inline gboolean
_is_write_head_less_than_max_size(QDisk *self)
{
  return self->hdr->write_head < self->hdr->disk_buf_size;
}

static inline gboolean
_is_able_to_reset_write_head_to_beginning_of_qdisk(QDisk *self)
{
  return self->hdr->backlog_head != QDISK_RESERVED_SPACE;
}

static inline gboolean
_is_free_space_between_write_head_and_backlog_head(QDisk *self, gint msg_len)
{
  /* this forces 1 byte of empty space between backlog and write */
  return self->hdr->write_head + msg_len < self->hdr->backlog_head;
}

static inline gboolean
_is_free_space_at_the_beginning_of_qdisk(QDisk *self, gint msg_len)
{
  /* this forces 1 byte of empty space between backlog and the beginning */
  return QDISK_RESERVED_SPACE + msg_len < self->hdr->backlog_head;
}

gboolean
qdisk_is_file_empty(QDisk *self)
{
  return self->hdr->length == 0 && self->hdr->backlog_len == 0;
}

gboolean
qdisk_is_space_avail(QDisk *self, gint at_least)
{
  if (_does_backlog_head_precede_write_head(self))
    {
      /* no exact size-check is needed in this case, because writing after
       * disk_buf_size is allowed when the last message does not fit in
       */
      if (_is_write_head_less_than_max_size(self))
        return TRUE;

      /* exact size-check is needed as we have unread/unacked data after the write head
       * that is being reset
       */
      return _is_able_to_reset_write_head_to_beginning_of_qdisk(self)
             && _is_free_space_at_the_beginning_of_qdisk(self, at_least);
    }

  return _is_free_space_between_write_head_and_backlog_head(self, at_least);
}

static inline gboolean
_ftruncate_would_reduce_file(QDisk *self, gint64 expected_size)
{
  gint64 expected_size_change = expected_size - self->file_size;
  return expected_size_change < 0;
}

static inline gboolean
_possible_size_reduction_reaches_truncate_threshold(QDisk *self, gint64 expected_size)
{
  gint64 possible_size_reduction = self->file_size - expected_size;
  gint64 truncate_threshold = (gint64)(qdisk_get_maximum_size(self) * self->options->truncate_size_ratio);
  return possible_size_reduction >= truncate_threshold;
}

static void
_maybe_truncate_file(QDisk *self, gint64 expected_size)
{
  if (_ftruncate_would_reduce_file(self, expected_size) &&
      !_possible_size_reduction_reaches_truncate_threshold(self, expected_size) &&
      G_LIKELY(!self->hdr->use_v1_wrap_condition))
    {
      return;
    }

  msg_debug("Truncating queue file", evt_tag_str("filename", self->filename), evt_tag_long("new size", expected_size));

  if (ftruncate(self->fd, (off_t) expected_size) == 0)
    {
      self->file_size = expected_size;
      return;
    }

  struct stat st;
  if (fstat(self->fd, &st) < 0)
    {
      msg_error("truncate file: cannot stat", evt_tag_error("error"));
    }
  else
    {
      self->file_size = (gint64) st.st_size;
    }

  msg_error("Error truncating disk-queue file",
            evt_tag_error("error"),
            evt_tag_str("filename", self->filename),
            evt_tag_long("expected-size", expected_size),
            evt_tag_long("file-size", self->file_size),
            evt_tag_int("fd", self->fd));
}

#if SYSLOG_NG_HAVE_POSIX_FALLOCATE
static gboolean
_posix_preallocate(QDisk *self, gint64 size)
{
  gint result = posix_fallocate(self->fd, 0, (off_t) size);
  if (result == 0)
    {
      self->file_size = size;
      return TRUE;
    }

  msg_error("Failed to preallocate queue file",
            evt_tag_str("filename", self->filename),
            evt_tag_errno("error", result));

  return FALSE;
}

#else
static gboolean
_compat_preallocate(QDisk *self, gint64 size)
{
  const size_t buf_size = QDISK_RESERVED_SPACE;
  gint64 buf_write_iterations = size / buf_size;
  gint64 additional_write_size = size - buf_write_iterations * buf_size;

  gchar buf[buf_size];
  for (gint i = 0; i < buf_size; i++)
    {
      buf[i] = '\0';
    }
  off_t pos = 0;

  for (gint i = 0; i < buf_write_iterations; i++)
    {
      if (!pwrite_strict(self->fd, buf, buf_size, pos))
        {
          msg_error("Failed to preallocate queue file",
                    evt_tag_str("filename", self->filename),
                    evt_tag_error("error"));
          return FALSE;
        }
      pos += buf_size;
      self->file_size = pos;
    }

  if (!pwrite_strict(self->fd, buf, additional_write_size, pos))
    {
      msg_error("Failed to preallocate queue file",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));
      return FALSE;
    }

  self->file_size = pos + additional_write_size;
  g_assert(self->file_size == size);

  return TRUE;
}
#endif

static gboolean
_preallocate(QDisk *self, gint64 size)
{
  msg_debug("Preallocating queue file",
            evt_tag_str("filename", self->filename),
            evt_tag_long("size", size));

#if SYSLOG_NG_HAVE_POSIX_FALLOCATE
  return _posix_preallocate(self, size);
#else
  return _compat_preallocate(self, size);
#endif
}

static inline gint64
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
_maybe_truncate_file_to_minimal(QDisk *self)
{
  if (qdisk_is_file_empty(self))
    {
      _maybe_truncate_file(self, QDISK_RESERVED_SPACE);
      return;
    }

  gint64 file_end_offset = qdisk_get_lowest_used_queue_offset(self);
  if (file_end_offset <= QDISK_RESERVED_SPACE)
    return;

  _maybe_truncate_file(self, file_end_offset);
}


gint64
qdisk_get_empty_space(QDisk *self)
{
  gint64 wpos = qdisk_get_writer_head(self);
  gint64 bpos = qdisk_get_backlog_head(self);

  if (wpos > bpos)
    {
      return (qdisk_get_maximum_size(self) - wpos) +
             (bpos - QDISK_RESERVED_SPACE);
    }

  return bpos - wpos;
}

static inline gboolean
_could_not_wrap_write_head_last_push_but_now_can(QDisk *self)
{
  return _has_position_reached_max_size(self, self->hdr->write_head)
         && _is_able_to_reset_write_head_to_beginning_of_qdisk(self);
}

gint64
qdisk_get_next_tail_position(QDisk *self)
{
  if (_could_not_wrap_write_head_last_push_but_now_can(self))
    return QDISK_RESERVED_SPACE;

  return self->hdr->write_head;
}

gboolean
qdisk_push_tail(QDisk *self, GString *record)
{
  if (!qdisk_started(self))
    return FALSE;

  if (_could_not_wrap_write_head_last_push_but_now_can(self))
    {
      /*
       * We can safely move the write_head to the beginning, but still
       * not sure, if this message will have space. We move the write_head
       * then check the available space compared to the new position.
       */
      self->hdr->write_head = QDISK_RESERVED_SPACE;
    }

  if (!qdisk_is_space_avail(self, record->len))
    return FALSE;

  if (!pwrite_strict(self->fd, record->str, record->len, self->hdr->write_head))
    {
      msg_error("Error writing disk-queue file",
                evt_tag_error("error"));
      return FALSE;
    }

  self->hdr->write_head = self->hdr->write_head + record->len;


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

  if (self->hdr->write_head > MAX(self->hdr->backlog_head, self->hdr->read_head))
    {
      if (self->file_size > self->hdr->write_head)
        {
          _maybe_truncate_file(self, self->hdr->write_head);
        }
      else
        {
          self->file_size = self->hdr->write_head;
        }

      if (_has_position_reached_max_size(self, self->hdr->write_head)
          && _is_able_to_reset_write_head_to_beginning_of_qdisk(self))
        {
          /* we were appending to the file, we are over the limit, and space
           * is available before the read head. truncate and wrap.
           *
           * Otherwise try to wrap again in the beginning of the next push.
           *
           * This way we guarantee, that only a part of 1 message is written after
           * disk_buf_size.
           */
          self->hdr->write_head = QDISK_RESERVED_SPACE;
        }
    }
  self->hdr->length++;
  return TRUE;
}

static inline gssize
_read_record_length_from_disk(QDisk *self, gint64 position, guint32 *record_length)
{
  gssize bytes_read = pread(self->fd, (gchar *)record_length, sizeof(guint32), position);

  *record_length = GUINT32_FROM_BE(*record_length);

  return bytes_read;
}

static inline gboolean
_is_record_length_reached_hard_limit(guint32 record_length)
{
  return record_length > MAX_RECORD_LENGTH;
}

static inline gssize
_is_record_length_valid(QDisk *self, gssize bytes_read, guint32 record_length, gint64 position)
{
  if (bytes_read != sizeof(record_length))
    {
      msg_error("Error reading disk-queue file, cannot read record-length",
                evt_tag_str("error", bytes_read < 0 ? g_strerror(errno) : "short read"),
                evt_tag_str("filename", self->filename),
                evt_tag_long("offset", position));
      return FALSE;
    }

  if (_is_record_length_reached_hard_limit(record_length))
    {
      msg_warning("Disk-queue file contains possibly invalid record-length",
                  evt_tag_int("rec_length", record_length),
                  evt_tag_str("filename", self->filename),
                  evt_tag_long("offset", position));
      return FALSE;
    }

  if (record_length == 0)
    {
      msg_error("Disk-queue file contains empty record",
                evt_tag_int("rec_length", record_length),
                evt_tag_str("filename", self->filename),
                evt_tag_long("offset", position));
      return FALSE;
    }

  return TRUE;
}

static inline gboolean
_try_reading_record_length(QDisk *self, gint64 position, guint32 *record_length)
{
  guint32 read_record_length;
  gssize bytes_read = _read_record_length_from_disk(self, position, &read_record_length);

  if (!_is_record_length_valid(self, bytes_read, read_record_length, position))
    return FALSE;

  *record_length = read_record_length;
  return TRUE;
}

static inline gboolean
_read_record_from_disk(QDisk *self, GString *record, guint32 record_length)
{
  g_string_set_size(record, record_length);

  gssize bytes_read = pread(self->fd, record->str, record_length, self->hdr->read_head + sizeof(record_length));
  if (bytes_read != record_length)
    {
      msg_error("Error reading disk-queue file",
                evt_tag_str("filename", self->filename),
                evt_tag_str("error", bytes_read < 0 ? g_strerror(errno) : "short read"),
                evt_tag_int("expected read length", record_length),
                evt_tag_int("actually read", bytes_read));
      return FALSE;
    }

  return TRUE;
}

static inline void
_maybe_apply_non_reliable_corrections(QDisk *self)
{
  if (self->options->reliable)
    return;

  qdisk_empty_backlog(self);
  if (!self->options->read_only)
    qdisk_reset_file_if_empty(self);
}


static inline void
_update_position_after_read(QDisk *self, guint32 record_length, gint64 *position)
{
  gint64 new_position = *position + record_length + sizeof(record_length);

  if (new_position > self->hdr->write_head)
    new_position = _correct_position_if_max_size_is_reached(self, new_position);

  *position = new_position;
}

gint64
qdisk_get_next_head_position(QDisk *self)
{
  gint64 next_read_head_position = self->hdr->read_head;
  if (next_read_head_position > self->hdr->write_head)
    next_read_head_position = _correct_position_if_max_size_is_reached(self, next_read_head_position);

  return next_read_head_position;
}

gboolean
qdisk_pop_head(QDisk *self, GString *record)
{
  if (self->hdr->read_head == self->hdr->write_head)
    return FALSE;

  if (self->hdr->read_head > self->hdr->write_head)
    self->hdr->read_head = _correct_position_if_max_size_is_reached(self, self->hdr->read_head);

  guint32 record_length;
  if (!_try_reading_record_length(self, self->hdr->read_head, &record_length))
    return FALSE;

  if (!_read_record_from_disk(self, record, record_length))
    return FALSE;

  _update_position_after_read(self, record_length, &self->hdr->read_head);
  self->hdr->length--;
  self->hdr->backlog_len++;

  _maybe_apply_non_reliable_corrections(self);
  return TRUE;
}

static gboolean
_skip_record(QDisk *self, gint64 position, gint64 *new_position)
{
  if (position == self->hdr->write_head)
    return FALSE;

  if (position > self->hdr->write_head)
    position = _correct_position_if_max_size_is_reached(self, position);

  *new_position = position;

  guint32 record_length;
  if (!_try_reading_record_length(self, *new_position, &record_length))
    return FALSE;

  _update_position_after_read(self, record_length, new_position);
  return TRUE;
}

gboolean
qdisk_remove_head(QDisk *self)
{
  gboolean success = _skip_record(self, self->hdr->read_head, &self->hdr->read_head);

  if (success)
    {
      self->hdr->length--;
      self->hdr->backlog_len++;
      _maybe_apply_non_reliable_corrections(self);
    }

  return success;
}

gboolean
qdisk_ack_backlog(QDisk *self)
{
  if (self->hdr->backlog_len == 0)
    return FALSE;

  if (!_skip_record(self, self->hdr->backlog_head, &self->hdr->backlog_head))
    {
      msg_error("Error acking in disk-queue file", evt_tag_str("filename", qdisk_get_filename(self)));
      return FALSE;
    }

  self->hdr->backlog_len--;
  return TRUE;
}


gboolean
qdisk_rewind_backlog(QDisk *self, guint rewind_count)
{
  if (rewind_count > self->hdr->backlog_len)
    return FALSE;

  gint64 number_of_messages_stay_in_backlog = self->hdr->backlog_len - rewind_count;

  gint64 new_read_head = self->hdr->backlog_head;
  for (gint64 i = 0; i < number_of_messages_stay_in_backlog; i++)
    {
      if (!_skip_record(self, new_read_head, &new_read_head))
        {
          msg_error("Error rewinding backlog in disk-queue file",
                    evt_tag_str("filename", qdisk_get_filename(self)));
          return FALSE;
        }
    }

  self->hdr->backlog_len = number_of_messages_stay_in_backlog;
  self->hdr->read_head = new_read_head;
  self->hdr->length = self->hdr->length + rewind_count;
  return TRUE;
}

void
qdisk_empty_backlog(QDisk *self)
{
  self->hdr->backlog_head = self->hdr->read_head;
  self->hdr->backlog_len = 0;
}

static gboolean
_overwrite_with_real_record_length(GString *serialized)
{
  guint32 record_length = GUINT32_TO_BE(serialized->len - sizeof(guint32));
  if (record_length == 0)
    return FALSE;

  g_string_overwrite_len(serialized, 0, (gchar *) &record_length, sizeof(guint32));
  return TRUE;
}

gboolean
qdisk_serialize(GString *serialized, QDiskSerializeFunc serialize_func, gpointer user_data, GError **error)
{
  SerializeArchive *sa = serialize_string_archive_new(serialized);

  /* Leave space for the real record_length for later */
  if (!serialize_write_uint32(sa, 0))
    {
      g_set_error(error, QDISK_ERROR, QDISK_ERROR_SERIALIZE, "failed to write record length");
      goto exit;
    }

  if (!serialize_func(sa, user_data))
    {
      g_set_error(error, QDISK_ERROR, QDISK_ERROR_SERIALIZE, "failed to serialize data");
      goto exit;
    }

  if (!_overwrite_with_real_record_length(serialized))
    {
      g_set_error(error, QDISK_ERROR, QDISK_ERROR_SERIALIZE, "serializable data is empty");
      goto exit;
    }

exit:
  serialize_archive_free(sa);
  return *error == NULL;
}

gboolean
qdisk_deserialize(GString *serialized, QDiskDeSerializeFunc deserialize_func, gpointer user_data, GError **error)
{
  SerializeArchive *sa = serialize_string_archive_new(serialized);

  if (!deserialize_func(sa, user_data))
    {
      g_set_error(error, QDISK_ERROR, QDISK_ERROR_DESERIALIZE, "failed to deserialize data");
      goto exit;
    }

exit:
  serialize_archive_free(sa);
  return *error == NULL;
}

static FILE *
_create_stream(QDisk *self, gint64 offset)
{
  int fd_copy = dup(self->fd);
  FILE *f = fdopen(fd_copy, "r");
  if (!f)
    {
      msg_error("Error opening file stream",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));
      close(fd_copy);
      return NULL;
    }

  if (fseek(f, offset, SEEK_SET) != 0)
    {
      msg_error("diskq-serializer: error while seeking in file stream",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));
      fclose(f);
      return NULL;
    }

  return f;
}

static gboolean
_load_queue(QDisk *self, GQueue *q, gint64 q_ofs, guint32 q_len, guint32 q_count)
{
  if (q_ofs)
    {
      FILE *f = _create_stream(self, q_ofs);
      if (!f)
        return FALSE;

      SerializeArchive *sa = serialize_file_archive_new(f);
      for (guint32 i = 0; i < q_count; i++)
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
                        evt_tag_long("num_of_messages", q_count),
                        evt_tag_long("invalid_index", i),
                        evt_tag_int("lost_messages", q_count - i));
              log_msg_unref(msg);
              break;
            }
        }
      serialize_archive_free(sa);
      if (fclose(f) != 0)
        msg_warning("Error closing file stream",
                    evt_tag_str("filename", self->filename),
                    evt_tag_error("error"));
    }
  return TRUE;
}

static gboolean
_try_to_load_queue(QDisk *self, GQueue *queue, QDiskQueuePosition *pos, gchar *type)
{
  gint64 ofs;
  guint32 count, len;

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

static gint64
_number_of_messages(QDisk *self)
{
  if (self->options->reliable)
    {
      return self->hdr->length + self->hdr->backlog_len;
    }
  else
    {
      return self->hdr->length +
             self->hdr->qbacklog_pos.count +
             self->hdr->qout_pos.count +
             self->hdr->qoverflow_pos.count;
    }
}

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
_save_queue(QDisk *self, GQueue *q, QDiskQueuePosition *q_pos)
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
      q_pos->ofs = 0;
      q_pos->len = 0;
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
      log_msg_serialize(msg, sa, 0);
      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
      if (string_reached_memory_limit(serialized))
        {
          if (!qdisk_write_serialized_string_to_file(self, serialized, &current_offset))
            goto error;
          if (!queue_start_position)
            queue_start_position = current_offset;
          written_bytes += serialized->len;
          g_string_truncate(serialized, 0);
        }
    }
  if (serialized->len)
    {
      if (!qdisk_write_serialized_string_to_file(self, serialized, &current_offset))
        goto error;
      if (!queue_start_position)
        queue_start_position = current_offset;
      written_bytes += serialized->len;
    }

  q_pos->len = written_bytes;
  q_pos->ofs = queue_start_position;
  success = TRUE;
error:
  g_string_free(serialized, TRUE);
  serialize_archive_free(sa);
  return success;
}

gboolean
qdisk_save_state(QDisk *self, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  QDiskQueuePosition qout_pos = { 0 };
  QDiskQueuePosition qbacklog_pos = { 0 };
  QDiskQueuePosition qoverflow_pos = { 0 };

  if (!self->options->reliable)
    {
      qout_pos.count = qout->length / 2;
      qbacklog_pos.count = qbacklog->length / 2;
      qoverflow_pos.count = qoverflow->length / 2;

      if (!_save_queue(self, qout, &qout_pos) ||
          !_save_queue(self, qbacklog, &qbacklog_pos) ||
          !_save_queue(self, qoverflow, &qoverflow_pos))
        return FALSE;
    }

  memcpy(self->hdr->magic, self->file_id, sizeof(self->hdr->magic));

  self->hdr->qout_pos = qout_pos;
  self->hdr->qbacklog_pos = qbacklog_pos;
  self->hdr->qoverflow_pos= qoverflow_pos;

  if (!self->options->reliable)
    msg_info("Disk-buffer state saved",
             evt_tag_str("filename", self->filename),
             evt_tag_long("qout_length", qout_pos.count),
             evt_tag_long("qbacklog_length", qbacklog_pos.count),
             evt_tag_long("qoverflow_length", qoverflow_pos.count),
             evt_tag_long("qdisk_length", self->hdr->length));
  else
    msg_info("Reliable disk-buffer state saved",
             evt_tag_str("filename", self->filename),
             evt_tag_long("qdisk_length", self->hdr->length));

  return TRUE;
}

static void
_close_file(QDisk *self)
{
  if (self->hdr)
    {
      if (self->options->read_only)
        g_free(self->hdr);
      else
        munmap((void *) self->hdr, sizeof(QDiskFileHeader));

      self->hdr = NULL;
    }

  if (self->fd != -1)
    {
      close(self->fd);
      self->fd = -1;
    }
}

static void
_ensure_header_byte_order(QDisk *self)
{
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
      self->hdr->disk_buf_size = GUINT64_SWAP_LE_BE(self->hdr->disk_buf_size);
      self->hdr->big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
    }
}

static gboolean
_create_path(const gchar *filename)
{
  FilePermOptions fpermoptions;
  file_perm_options_defaults(&fpermoptions);
  return file_perm_options_create_containing_directory(&fpermoptions, filename);
}

static gboolean
_open_file(QDisk *self, const gchar *filename)
{
  gint fd = open(filename, O_LARGEFILE | (self->options->read_only ? O_RDONLY : O_RDWR), 0600);
  if (fd < 0)
    {
      msg_error("Error opening disk-queue file",
                evt_tag_str("filename", filename),
                evt_tag_error("error"));
      return FALSE;
    }

  self->fd = fd;
  self->filename = g_strdup(filename);

  struct stat st;
  if (fstat(self->fd, &st) != 0 || st.st_size == 0)
    {
      msg_error("Error loading disk-queue file. Cannot stat",
                evt_tag_str("filename", self->filename),
                evt_tag_error("fstat error"),
                evt_tag_int("size", st.st_size));
      _close_file(self);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_create_file(QDisk *self, const gchar *filename)
{
  g_assert(!self->options->read_only);
  g_assert(filename);

  if (self->options->disk_buf_size == -1)
    {
      msg_error("disk-buf-size for new disk-queue files must be set");
      return FALSE;
    }

  if (!_create_path(filename))
    {
      msg_error("Error creating dir for disk-queue file",
                evt_tag_str("filename", filename),
                evt_tag_error("error"));
      return FALSE;
    }

  gint fd = open(filename, O_RDWR | O_LARGEFILE | O_CREAT, 0600);
  if (fd < 0)
    {
      msg_error("Error creating disk-queue file",
                evt_tag_str("filename", filename),
                evt_tag_error("error"));
      return FALSE;
    }

  self->fd = fd;
  self->filename = g_strdup(filename);

  if (self->options->prealloc)
    return _preallocate(self, self->options->disk_buf_size);

  return TRUE;
}

static gboolean
_create_header(QDisk *self)
{
  self->hdr = (QDiskFileHeader *) mmap(0, sizeof(QDiskFileHeader), (PROT_READ | PROT_WRITE), MAP_SHARED, self->fd, 0);

  if (self->hdr == MAP_FAILED)
    {
      msg_error("Error returned by mmap", evt_tag_error("errno"));
      return FALSE;
    }

  madvise(self->hdr, sizeof(QDiskFileHeader), MADV_RANDOM);

  QDiskFileHeader nulled_hdr;
  memset(&nulled_hdr, 0, sizeof(nulled_hdr));
  if (!pwrite_strict(self->fd, &nulled_hdr, sizeof(nulled_hdr), 0))
    {
      msg_error("Error occurred while allocating the header for a new queue file",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));
      return FALSE;
    }

  memcpy(self->hdr->magic, self->file_id, sizeof(self->hdr->magic));

  self->hdr->version = QDISK_HDR_VERSION_CURRENT;
  self->hdr->big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);

  self->hdr->read_head = QDISK_RESERVED_SPACE;
  self->hdr->write_head = QDISK_RESERVED_SPACE;
  self->hdr->backlog_head = QDISK_RESERVED_SPACE;
  self->hdr->backlog_len = 0;
  self->hdr->length = 0;
  self->hdr->use_v1_wrap_condition = FALSE;
  self->hdr->disk_buf_size = self->options->disk_buf_size;

  return TRUE;
}

static inline gboolean
_is_header_version_current(QDisk *self)
{
  return self->hdr->version == QDISK_HDR_VERSION_CURRENT;
}

static void
_upgrade_header(QDisk *self)
{
  if (self->hdr->version == 0)
    {
      self->hdr->big_endian = TRUE;
      qdisk_empty_backlog(self);
    }

  if (self->hdr->version < 2)
    {
      struct stat st;
      gboolean file_was_overwritten = (fstat(self->fd, &st) != 0 || st.st_size > self->hdr->disk_buf_size);
      self->hdr->use_v1_wrap_condition = file_was_overwritten;
    }

  if (self->hdr->version < 3)
    {
      self->hdr->disk_buf_size = self->options->disk_buf_size;
    }

  self->hdr->version = QDISK_HDR_VERSION_CURRENT;
}

static gboolean
_load_header(QDisk *self)
{
  QDiskFileHeader *hdr_mmapped = (QDiskFileHeader *) mmap(0, sizeof(QDiskFileHeader),
                                                          self->options->read_only ? (PROT_READ) : (PROT_READ | PROT_WRITE),
                                                          MAP_SHARED, self->fd, 0);
  if (hdr_mmapped == MAP_FAILED)
    {
      msg_error("Error returned by mmap", evt_tag_error("errno"));
      return FALSE;
    }

  madvise(hdr_mmapped, sizeof(QDiskFileHeader), MADV_RANDOM);

  if (self->options->read_only)
    {
      QDiskFileHeader *hdr_allocated = g_malloc(sizeof(QDiskFileHeader));
      memcpy(hdr_allocated, hdr_mmapped, sizeof(QDiskFileHeader));
      munmap(hdr_mmapped, sizeof(QDiskFileHeader));
      self->hdr = hdr_allocated;
    }
  else
    {
      self->hdr = hdr_mmapped;
    }

  if (!_is_header_version_current(self))
    _upgrade_header(self);

  _ensure_header_byte_order(self);

  if (memcmp(self->hdr->magic, self->file_id, 4) != 0)
    {
      msg_error("Error reading disk-queue file header. Invalid magic",
                evt_tag_str("filename", self->filename));
      return FALSE;
    }

  if (qdisk_header_is_inconsistent(self))
    {
      msg_error("Inconsistent header data in disk-queue file, ignoring",
                evt_tag_str("filename", self->filename),
                evt_tag_long("read_head", self->hdr->read_head),
                evt_tag_long("write_head", self->hdr->write_head),
                evt_tag_long("qdisk_length",  self->hdr->length));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_load_state(QDisk *self, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  if (!_load_header(self))
    return FALSE;

  if (!self->options->reliable)
    {
      if (!_load_non_reliable_queues(self, qout, qbacklog, qoverflow))
        return FALSE;

      self->file_size = QDISK_RESERVED_SPACE;
      if (!self->options->read_only)
        {
          _maybe_truncate_file_to_minimal(self);
        }

      msg_info("Disk-buffer state loaded",
               evt_tag_str("filename", self->filename),
               evt_tag_long("number_of_messages", _number_of_messages(self)));

      msg_debug("Disk-buffer internal state",
                evt_tag_str("filename", self->filename),
                evt_tag_long("qout_length", self->hdr->qout_pos.count),
                evt_tag_long("qbacklog_length", self->hdr->qbacklog_pos.count),
                evt_tag_long("qoverflow_length", self->hdr->qoverflow_pos.count),
                evt_tag_long("qdisk_length", self->hdr->length),
                evt_tag_long("read_head", self->hdr->read_head),
                evt_tag_long("write_head", self->hdr->write_head),
                evt_tag_long("disk_buf_size", self->hdr->disk_buf_size));

      _reset_queue_pointers(self);
    }
  else
    {
      struct stat st;
      fstat(self->fd, &st);
      self->file_size = st.st_size;
      msg_info("Reliable disk-buffer state loaded",
               evt_tag_str("filename", self->filename),
               evt_tag_long("number_of_messages", _number_of_messages(self)));

      msg_debug("Reliable disk-buffer internal state",
                evt_tag_str("filename", self->filename),
                evt_tag_long("queue_length", self->hdr->length),
                evt_tag_long("backlog_len", self->hdr->backlog_len),
                evt_tag_long("backlog_head", self->hdr->backlog_head),
                evt_tag_long("read_head", self->hdr->read_head),
                evt_tag_long("write_head", self->hdr->write_head),
                evt_tag_long("disk_buf_size", self->hdr->disk_buf_size));
    }

  return TRUE;
}

gboolean
_autodetect_disk_buf_size(QDisk *self)
{
  struct stat st;
  if (fstat(self->fd, &st) < 0)
    {
      msg_error("Autodetect disk-buf-size(): cannot stat",
                evt_tag_str("filename", self->filename),
                evt_tag_error("error"));

      return FALSE;
    }

  if (qdisk_is_file_empty(self))
    {
      self->hdr->disk_buf_size = MAX(MIN_DISK_BUF_SIZE, st.st_size);

      msg_debug("Autodetected empty disk-queue's disk-buf-size()",
                evt_tag_str("filename", self->filename),
                evt_tag_long("disk_buf_size", self->hdr->disk_buf_size));

      return TRUE;
    }

  if (self->hdr->write_head > MAX(self->hdr->backlog_head, self->hdr->read_head))
    {
      self->hdr->disk_buf_size = (gint64) st.st_size;

      msg_debug("Autodetected disk-buf-size()",
                evt_tag_str("filename", self->filename),
                evt_tag_long("disk_buf_size", self->hdr->disk_buf_size));

      return TRUE;
    }

  msg_error("Failed to autodetect disk-buf-size() as the disk-queue file is wrapped",
            evt_tag_str("filename", self->filename));

  return FALSE;
}

static gboolean
_ensure_disk_buf_size(QDisk *self)
{
  if (self->hdr->disk_buf_size == -1 && !_autodetect_disk_buf_size(self))
    return FALSE;

  if (self->options->disk_buf_size != -1 && self->hdr->disk_buf_size != self->options->disk_buf_size)
    {
      msg_warning("WARNING: disk-buf-size() has changed since the last syslog-ng run. syslog-ng currently does "
                  "not support changing the disk-buf-size() of existing disk-queues. Continuing with the old one",
                  evt_tag_str("filename", self->filename),
                  evt_tag_long("active_old_disk_buf_size", self->hdr->disk_buf_size),
                  evt_tag_long("ignored_new_disk_buf_size", self->options->disk_buf_size));
    }

  return TRUE;
}

static gboolean
_load_qdisk_file(QDisk *self, const gchar *filename, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  if (!_open_file(self, filename))
    goto error;

  if (!_load_state(self, qout, qbacklog, qoverflow))
    goto error;

  if (!_ensure_disk_buf_size(self))
    goto error;

  return TRUE;

error:
  _close_file(self);
  return FALSE;
}

static gboolean
_init_qdisk_file(QDisk *self)
{
  self->file_size = QDISK_RESERVED_SPACE;
  return _create_header(self);
}

gboolean
qdisk_start(QDisk *self, const gchar *filename, GQueue *qout, GQueue *qbacklog, GQueue *qoverflow)
{
  /*
   * If qdisk_start is called for already initialized qdisk file
   * it can cause message loosing.
   * We need this assert to detect programming error as soon as possible.
   */
  g_assert(!qdisk_started(self));

  struct stat st;
  gboolean file_exists = filename && stat(filename, &st) != -1;

  if (file_exists)
    return _load_qdisk_file(self, filename, qout, qbacklog, qoverflow);

  if (filename)
    {
      if (!_create_file(self, filename))
        return FALSE;
    }
  else
    {
      gchar next_filename[256];
      if (_next_filename(self, next_filename, sizeof(next_filename)))
        {
          if (!_create_file(self, next_filename))
            return FALSE;
        }
    }

  if (!_init_qdisk_file(self))
    {
      _close_file(self);
      return FALSE;
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
  g_free(self->filename);
  self->filename = NULL;

  _close_file(self);
}

void
qdisk_reset_file_if_empty(QDisk *self)
{
  if (!qdisk_is_file_empty(self))
    return;

  self->hdr->read_head = QDISK_RESERVED_SPACE;
  self->hdr->write_head = QDISK_RESERVED_SPACE;
  self->hdr->backlog_head = QDISK_RESERVED_SPACE;

  _maybe_truncate_file(self, QDISK_RESERVED_SPACE);
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

gint64
qdisk_get_maximum_size(QDisk *self)
{
  return self->hdr->disk_buf_size;
}

const gchar *
qdisk_get_filename(QDisk *self)
{
  return self->filename;
}

gint64
qdisk_get_file_size(QDisk *self)
{
  return self->file_size;
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

gint64
qdisk_get_backlog_head(QDisk *self)
{
  return self->hdr->backlog_head;
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
  self->options = NULL;
  g_free(self);
}

QDisk *
qdisk_new(DiskQueueOptions *options, const gchar *file_id)
{
  QDisk *self = g_new0(QDisk, 1);
  qdisk_init_instance(self, options, file_id);
  return self;
}
