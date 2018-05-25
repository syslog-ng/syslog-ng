/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "logproto-buffered-server.h"
#include "messages.h"
#include "serialize.h"
#include "compat/string.h"

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

typedef struct _BufferedServerBookmarkData
{
  PersistEntryHandle persist_handle;
  gint32 pending_buffer_pos;
  gint64 pending_raw_stream_pos;
  gint32 pending_raw_buffer_size;
} BufferedServerBookmarkData;

LogProtoBufferedServerState *
log_proto_buffered_server_get_state(LogProtoBufferedServer *self)
{
  if (self->persist_state)
    {
      g_assert(self->persist_handle != 0);
      return persist_state_map_entry(self->persist_state, self->persist_handle);
    }
  if (G_UNLIKELY(!self->state1))
    {
      self->state1 = g_new0(LogProtoBufferedServerState, 1);
    }
  return self->state1;
}

void
log_proto_buffered_server_put_state(LogProtoBufferedServer *self)
{
  if (self->persist_state && self->persist_handle)
    persist_state_unmap_entry(self->persist_state, self->persist_handle);
}

static gboolean
log_proto_buffered_server_convert_from_raw(LogProtoBufferedServer *self, const guchar *raw_buffer, gsize raw_buffer_len)
{
  /* some data was read */
  gsize avail_in = raw_buffer_len;
  gsize avail_out;
  gchar *out;
  gint  ret = -1;
  gboolean success = FALSE;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);

  do
    {
      avail_out = state->buffer_size - state->pending_buffer_end;
      out = (gchar *) self->buffer + state->pending_buffer_end;

      ret = g_iconv(self->convert, (gchar **) &raw_buffer, &avail_in, (gchar **) &out, &avail_out);
      if (ret == (gsize) -1)
        {
          switch (errno)
            {
            case EINVAL:
              if (self->stream_based)
                {
                  /* Incomplete text, do not report an error, rather try to read again */
                  state->pending_buffer_end = state->buffer_size - avail_out;

                  if (avail_in > 0)
                    {
                      if (avail_in > sizeof(state->raw_buffer_leftover))
                        {
                          msg_error("Invalid byte sequence, the remaining raw buffer is larger than the supported leftover size",
                                    evt_tag_str("encoding", self->super.options->encoding),
                                    evt_tag_int("avail_in", avail_in),
                                    evt_tag_int("leftover_size", sizeof(state->raw_buffer_leftover)));
                          goto error;
                        }
                      memcpy(state->raw_buffer_leftover, raw_buffer, avail_in);
                      state->raw_buffer_leftover_size = avail_in;
                      state->raw_buffer_size -= avail_in;
                      msg_trace("Leftover characters remained after conversion, delaying message until another chunk arrives",
                                evt_tag_str("encoding", self->super.options->encoding),
                                evt_tag_int("avail_in", avail_in));
                      goto success;
                    }
                }
              else
                {
                  msg_error("Byte sequence too short, cannot convert an individual frame in its entirety",
                            evt_tag_str("encoding", self->super.options->encoding),
                            evt_tag_int("avail_in", avail_in));
                  goto error;
                }
              break;
            case E2BIG:
              state->pending_buffer_end = state->buffer_size - avail_out;
              /* extend the buffer */

              if (state->buffer_size < self->super.options->max_buffer_size)
                {
                  state->buffer_size *= 2;
                  if (state->buffer_size > self->super.options->max_buffer_size)
                    state->buffer_size = self->super.options->max_buffer_size;

                  self->buffer = g_realloc(self->buffer, state->buffer_size);

                  /* recalculate the out pointer, and add what we have now */
                  ret = -1;
                }
              else
                {
                  msg_error("Incoming byte stream requires a too large conversion buffer, probably invalid character sequence",
                            evt_tag_str("encoding", self->super.options->encoding),
                            evt_tag_printf("buffer", "%.*s", (gint) state->pending_buffer_end, self->buffer));
                  goto error;
                }
              break;
            case EILSEQ:
            default:
              msg_notice("Invalid byte sequence or other error while converting input, skipping character",
                         evt_tag_str("encoding", self->super.options->encoding),
                         evt_tag_printf("char", "0x%02x", *(guchar *) raw_buffer));
              goto error;
            }
        }
      else
        {
          state->pending_buffer_end = state->buffer_size - avail_out;
        }
    }
  while (avail_in > 0);

success:
  success = TRUE;
error:
  log_proto_buffered_server_put_state(self);
  return success;
}

static void
log_proto_buffered_server_apply_state(LogProtoBufferedServer *self, PersistEntryHandle handle,
                                      const gchar *persist_name)
{
  struct stat st;
  gint64 ofs = 0;
  LogProtoBufferedServerState *state;
  gint fd;

  fd = self->super.transport->fd;
  self->persist_handle = handle;

  if (fstat(fd, &st) < 0)
    return;

  state = log_proto_buffered_server_get_state(self);

  if (!self->buffer)
    {
      self->buffer = g_malloc(state->buffer_size);
    }
  state->pending_buffer_end = 0;

  if (state->file_inode &&
      state->file_inode == st.st_ino &&
      state->file_size <= st.st_size &&
      state->raw_stream_pos <= st.st_size)
    {
      ofs = state->raw_stream_pos;

      lseek(fd, ofs, SEEK_SET);
    }
  else
    {
      if (state->file_inode)
        {
          /* the stored state does not match the current file */
          msg_notice("The current log file has a mismatching size/inode information, restarting from the beginning",
                     evt_tag_str("state", persist_name),
                     evt_tag_int("stored_inode", state->file_inode),
                     evt_tag_int("cur_file_inode", st.st_ino),
                     evt_tag_int("stored_size", state->file_size),
                     evt_tag_int("cur_file_size", st.st_size),
                     evt_tag_int("raw_stream_pos", state->raw_stream_pos));
        }
      goto error;
    }
  if (state->raw_buffer_size)
    {
      gssize rc;
      guchar *raw_buffer;

      if (!self->super.options->encoding)
        {
          /* no conversion, we read directly into our buffer */
          if (state->raw_buffer_size > state->buffer_size)
            {
              msg_notice("Invalid LogProtoBufferedServerState.raw_buffer_size, larger than buffer_size and no encoding is set, restarting from the beginning",
                         evt_tag_str("state", persist_name),
                         evt_tag_int("raw_buffer_size", state->raw_buffer_size),
                         evt_tag_int("buffer_size", state->buffer_size),
                         evt_tag_int("init_buffer_size", self->super.options->init_buffer_size));
              goto error;
            }
          raw_buffer = self->buffer;
        }
      else
        {
          if (state->raw_buffer_size > self->super.options->max_buffer_size)
            {
              msg_notice("Invalid LogProtoBufferedServerState.raw_buffer_size, larger than max_buffer_size, restarting from the beginning",
                         evt_tag_str("state", persist_name),
                         evt_tag_int("raw_buffer_size", state->raw_buffer_size),
                         evt_tag_int("init_buffer_size", self->super.options->init_buffer_size),
                         evt_tag_int("max_buffer_size", self->super.options->max_buffer_size));
              goto error;
            }
          raw_buffer = g_alloca(state->raw_buffer_size);
        }

      rc = log_transport_read(self->super.transport, raw_buffer, state->raw_buffer_size, NULL);
      if (rc != state->raw_buffer_size)
        {
          msg_notice("Error re-reading buffer contents of the file to be continued, restarting from the beginning",
                     evt_tag_str("state", persist_name));
          goto error;
        }

      state->pending_buffer_end = 0;
      if (self->super.options->encoding)
        {
          if (!log_proto_buffered_server_convert_from_raw(self, raw_buffer, rc))
            {
              msg_notice("Error re-converting buffer contents of the file to be continued, restarting from the beginning",
                         evt_tag_str("state", persist_name));
              goto error;
            }
        }
      else
        {
          state->pending_buffer_end += rc;
        }

      if (state->buffer_pos > state->pending_buffer_end)
        {
          msg_notice("Converted buffer contents is smaller than the current buffer position, starting from the beginning of the buffer, some lines may be duplicated",
                     evt_tag_str("state", persist_name));
          state->buffer_pos = state->pending_buffer_pos = 0;
        }

      self->fetch_state = LPBSF_FETCHING_FROM_BUFFER;
    }
  else
    {
      /* although we do have buffer position information, but the
       * complete contents of the buffer is already processed, instead
       * of reading and then dropping it, position the file after the
       * indicated block */

      state->raw_stream_pos += state->raw_buffer_size;
      ofs = state->raw_stream_pos;
      state->raw_buffer_size = 0;
      state->buffer_pos = state->pending_buffer_end = 0;

      lseek(fd, state->raw_stream_pos, SEEK_SET);
    }
  goto exit;

error:
  ofs = 0;
  state->buffer_pos = 0;
  state->pending_buffer_end = 0;
  state->__deprecated_buffer_cached_eol = 0;
  state->raw_stream_pos = 0;
  state->raw_buffer_size = 0;
  state->raw_buffer_leftover_size = 0;
  lseek(fd, 0, SEEK_SET);

exit:
  state->file_inode = st.st_ino;
  state->file_size = st.st_size;
  state->raw_stream_pos = ofs;
  state->pending_buffer_pos = state->buffer_pos;
  state->pending_raw_stream_pos = state->raw_stream_pos;
  state->pending_raw_buffer_size = state->raw_buffer_size;
  state->__deprecated_buffer_cached_eol = 0;

  state = NULL;
  log_proto_buffered_server_put_state(self);
}

static PersistEntryHandle
log_proto_buffered_server_alloc_state(LogProtoBufferedServer *self, PersistState *persist_state,
                                      const gchar *persist_name)
{
  LogProtoBufferedServerState *state;
  PersistEntryHandle handle;

  handle = persist_state_alloc_entry(persist_state, persist_name, sizeof(LogProtoBufferedServerState));
  if (handle)
    {
      state = persist_state_map_entry(persist_state, handle);

      state->header.version = 0;
      state->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);

      persist_state_unmap_entry(persist_state, handle);

    }
  return handle;
}

static gboolean
log_proto_buffered_server_convert_state(LogProtoBufferedServer *self, guint8 persist_version, gpointer old_state,
                                        gsize old_state_size, LogProtoBufferedServerState *state)
{
  if (persist_version <= 2)
    {
      state->header.version = 0;
      state->file_inode = 0;
      state->raw_stream_pos = strtoll((gchar *) old_state, NULL, 10);
      state->file_size = 0;

      return TRUE;
    }
  else if (persist_version == 3)
    {
      SerializeArchive *archive;
      guint32 read_length;
      gint64 cur_size;
      gint64 cur_inode;
      gint64 cur_pos;
      guint16 version = 0;
      gchar *buffer;
      gsize buffer_len;

      cur_inode = -1;
      cur_pos = 0;
      cur_size = 0;
      version = 0;
      archive = serialize_buffer_archive_new(old_state, old_state_size);

      /* NOTE: the v23 conversion code adds an extra length field which we
       * need to read out. */
      g_assert(serialize_read_uint32(archive, &read_length) && read_length == old_state_size - sizeof(read_length));

      /* original v3 format starts here */
      if (!serialize_read_uint16(archive, &version) || version != 0)
        {
          msg_error("Internal error restoring log reader state, stored data has incorrect version",
                    evt_tag_int("version", version));
          goto error_converting_v3;
        }

      if (!serialize_read_uint64(archive, (guint64 *) &cur_pos) ||
          !serialize_read_uint64(archive, (guint64 *) &cur_inode) ||
          !serialize_read_uint64(archive, (guint64 *) &cur_size))
        {
          msg_error("Internal error restoring information about the current file position, restarting from the beginning");
          goto error_converting_v3;
        }

      if (!serialize_read_uint16(archive, &version) || version != 0)
        {
          msg_error("Internal error, protocol state has incorrect version",
                    evt_tag_int("version", version));
          goto error_converting_v3;
        }

      if (!serialize_read_cstring(archive, &buffer, &buffer_len))
        {
          msg_error("Internal error, error reading buffer contents",
                    evt_tag_int("version", version));
          goto error_converting_v3;
        }

      if (!self->buffer || state->buffer_size < buffer_len)
        {
          gsize buffer_size = MAX(self->super.options->init_buffer_size, buffer_len);
          self->buffer = g_realloc(self->buffer, buffer_size);
        }
      serialize_archive_free(archive);

      memcpy(self->buffer, buffer, buffer_len);
      state->buffer_pos = 0;
      state->pending_buffer_end = buffer_len;
      g_free(buffer);

      state->header.version = 0;
      state->file_inode = cur_inode;
      state->raw_stream_pos = cur_pos;
      state->file_size = cur_size;
      return TRUE;
error_converting_v3:
      serialize_archive_free(archive);
    }
  return FALSE;
}

gboolean
log_proto_buffered_server_restart_with_state(LogProtoServer *s, PersistState *persist_state, const gchar *persist_name)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  guint8 persist_version;
  PersistEntryHandle old_state_handle;
  gpointer old_state;
  gsize old_state_size;
  PersistEntryHandle new_state_handle = 0;
  gpointer new_state = NULL;
  gboolean success;

  self->persist_state = persist_state;
  old_state_handle = persist_state_lookup_entry(persist_state, persist_name, &old_state_size, &persist_version);
  if (!old_state_handle)
    {
      new_state_handle = log_proto_buffered_server_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;
      log_proto_buffered_server_apply_state(self, new_state_handle, persist_name);
      return TRUE;
    }
  if (persist_version < 4)
    {
      new_state_handle = log_proto_buffered_server_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;

      old_state = persist_state_map_entry(persist_state, old_state_handle);
      new_state = persist_state_map_entry(persist_state, new_state_handle);
      success = log_proto_buffered_server_convert_state(self, persist_version, old_state, old_state_size, new_state);
      persist_state_unmap_entry(persist_state, old_state_handle);
      persist_state_unmap_entry(persist_state, new_state_handle);

      /* we're using the newly allocated state structure regardless if
       * conversion succeeded. If the conversion went wrong then
       * new_state is still in its freshly initialized form since the
       * convert function will not touch the state in the error
       * branches.
       */

      log_proto_buffered_server_apply_state(self, new_state_handle, persist_name);
      return success;
    }
  else if (persist_version == 4)
    {
      LogProtoBufferedServerState *state;

      old_state = persist_state_map_entry(persist_state, old_state_handle);
      state = old_state;
      if ((state->header.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
          (!state->header.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
        {

          /* byte order conversion in order to avoid the hassle with
             scattered byte order conversions in the code */

          state->header.big_endian = !state->header.big_endian;
          state->buffer_pos = GUINT32_SWAP_LE_BE(state->buffer_pos);
          state->pending_buffer_pos = GUINT32_SWAP_LE_BE(state->pending_buffer_pos);
          state->pending_buffer_end = GUINT32_SWAP_LE_BE(state->pending_buffer_end);
          state->buffer_size = GUINT32_SWAP_LE_BE(state->buffer_size);
          state->raw_stream_pos = GUINT64_SWAP_LE_BE(state->raw_stream_pos);
          state->raw_buffer_size = GUINT32_SWAP_LE_BE(state->raw_buffer_size);
          state->pending_raw_stream_pos = GUINT64_SWAP_LE_BE(state->pending_raw_stream_pos);
          state->pending_raw_buffer_size = GUINT32_SWAP_LE_BE(state->pending_raw_buffer_size);
          state->file_size = GUINT64_SWAP_LE_BE(state->file_size);
          state->file_inode = GUINT64_SWAP_LE_BE(state->file_inode);
        }

      if (state->header.version > 0)
        {
          msg_error("Internal error restoring log reader state, stored data is too new",
                    evt_tag_int("version", state->header.version));
          goto error;
        }
      persist_state_unmap_entry(persist_state, old_state_handle);
      log_proto_buffered_server_apply_state(self, old_state_handle, persist_name);
      return TRUE;
    }
  else
    {
      msg_error("Internal error restoring log reader state, stored data is too new",
                evt_tag_int("version", persist_version));
      goto error;
    }
  return TRUE;
fallback_non_persistent:
  new_state = g_new0(LogProtoBufferedServerState, 1);
error:
  if (!new_state)
    {
      new_state_handle = log_proto_buffered_server_alloc_state(self, persist_state, persist_name);
      if (!new_state_handle)
        goto fallback_non_persistent;
      new_state = persist_state_map_entry(persist_state, new_state_handle);
    }
  if (new_state)
    {
      LogProtoBufferedServerState *state = new_state;

      /* error happened,  restart the file from the beginning */
      state->raw_stream_pos = 0;
      state->file_inode = 0;
      state->file_size = 0;
      if (new_state_handle)
        log_proto_buffered_server_apply_state(self, new_state_handle, persist_name);
      else
        {
          self->persist_state = NULL;
          self->state1 = new_state;
        }
    }
  if (new_state_handle)
    {
      persist_state_unmap_entry(persist_state, new_state_handle);
    }
  return FALSE;
}

LogProtoPrepareAction
log_proto_buffered_server_prepare(LogProtoServer *s, GIOCondition *cond, gint *timeout G_GNUC_UNUSED)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;

  *cond = self->super.transport->cond;

  /* if there's no pending I/O in the transport layer, then we want to do a read */
  if (*cond == 0)
    *cond = G_IO_IN;

  return LPPA_POLL_IO;
}

static gint
log_proto_buffered_server_read_data_method(LogProtoBufferedServer *self, guchar *buf, gsize len,
                                           LogTransportAuxData *aux)
{
  return log_transport_read(self->super.transport, buf, len, aux);
}

static gboolean
log_proto_buffered_server_fetch_from_buffer(LogProtoBufferedServer *self, const guchar **msg, gsize *msg_len,
                                            LogTransportAuxData *aux)
{
  gsize buffer_bytes;
  const guchar *buffer_start;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  gboolean success = FALSE;

  buffer_start = self->buffer + state->pending_buffer_pos;
  buffer_bytes = state->pending_buffer_end - state->pending_buffer_pos;

  if (buffer_bytes == 0)
    {
      /* if buffer_bytes is zero bytes, it means that we completely
       * processed our buffer without having a fraction of a line still
       * there.  It is important to reset
       * pending_buffer_pos/pending_buffer_end to zero as the caller assumes
       * that if we return no message from the buffer, then buffer_pos is
       * _zero_.
       */

      if (G_UNLIKELY(self->pos_tracking))
        {
          state->pending_raw_stream_pos += state->pending_raw_buffer_size;
          state->pending_raw_buffer_size = 0;
        }
      state->pending_buffer_pos = state->pending_buffer_end = 0;
      goto exit;
    }

  success = self->fetch_from_buffer(self, buffer_start, buffer_bytes, msg, msg_len);
  if (aux)
    log_transport_aux_data_copy(aux, &self->buffer_aux);
exit:
  log_proto_buffered_server_put_state(self);
  return success;
}

static inline void
log_proto_buffered_server_allocate_buffer(LogProtoBufferedServer *self, LogProtoBufferedServerState *state)
{
  state->buffer_size = self->super.options->init_buffer_size;
  self->buffer = g_malloc(state->buffer_size);
}

static inline gint
log_proto_buffered_server_read_data(LogProtoBufferedServer *self, gpointer buffer, gsize count)
{
  gint rc;

  log_transport_aux_data_reinit(&self->buffer_aux);
  rc = self->read_data(self, buffer, count, &self->buffer_aux);
  return rc;
}

static GIOStatus
log_proto_buffered_server_fetch_into_buffer(LogProtoBufferedServer *self)
{
  guchar *raw_buffer = NULL;
  gint avail;
  gint rc;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  GIOStatus result = G_IO_STATUS_NORMAL;

  if (G_UNLIKELY(!self->buffer))
    log_proto_buffered_server_allocate_buffer(self, state);

  if (self->convert == (GIConv) -1)
    {
      /* no conversion, we read directly into our buffer */
      raw_buffer = self->buffer + state->pending_buffer_end;
      avail = state->buffer_size - state->pending_buffer_end;
    }
  else
    {
      /* if conversion is needed, we first read into an on-stack
       * buffer, and then convert it into our internal buffer */

      raw_buffer = g_alloca(self->super.options->init_buffer_size + state->raw_buffer_leftover_size);
      memcpy(raw_buffer, state->raw_buffer_leftover, state->raw_buffer_leftover_size);
      avail = self->super.options->init_buffer_size;
    }

  if (avail == 0)
    goto exit;

  rc = log_proto_buffered_server_read_data(self, raw_buffer + state->raw_buffer_leftover_size, avail);
  if (rc < 0)
    {
      if (errno == EAGAIN)
        {
          /* ok we don't have any more data to read, return to main poll loop */
          result = G_IO_STATUS_AGAIN;
        }
      else
        {
          /* an error occurred while reading */
          msg_error("I/O error occurred while reading",
                    evt_tag_int(EVT_TAG_FD, self->super.transport->fd),
                    evt_tag_error(EVT_TAG_OSERROR));
          result = G_IO_STATUS_ERROR;
        }
    }
  else if (rc == 0)
    {
      /* EOF read */
      msg_verbose("EOF occurred while reading",
                  evt_tag_int(EVT_TAG_FD, self->super.transport->fd));
      if (state->raw_buffer_leftover_size > 0)
        {
          msg_error("EOF read on a channel with leftovers from previous character conversion, dropping input");
          state->pending_buffer_pos = state->pending_buffer_end = 0;
        }
      result = G_IO_STATUS_EOF;
    }
  else
    {
      state->pending_raw_buffer_size += rc;
      rc += state->raw_buffer_leftover_size;
      state->raw_buffer_leftover_size = 0;

      if (self->convert == (GIConv) -1)
        {
          state->pending_buffer_end += rc;
        }
      else if (!log_proto_buffered_server_convert_from_raw(self, raw_buffer, rc))
        {
          result = G_IO_STATUS_ERROR;
        }
    }
exit:
  log_proto_buffered_server_put_state(self);
  return result;
}

static LogProtoStatus
_convert_io_status_to_log_proto_status(GIOStatus io_status)
{
  if (io_status == G_IO_STATUS_EOF)
    return LPS_EOF;
  else if (io_status == G_IO_STATUS_ERROR)
    return LPS_ERROR;
  g_assert_not_reached();
}


static void
_buffered_server_update_pos(LogProtoServer *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);

  if (state->pending_buffer_pos == state->pending_buffer_end)
    {
      state->pending_buffer_end = 0;
      state->pending_buffer_pos = 0;
      if (self->pos_tracking)
        {
          state->pending_raw_stream_pos += state->pending_raw_buffer_size;
          state->pending_raw_buffer_size = 0;
        }
    }

  log_proto_buffered_server_put_state(self);
}

static void
_buffered_server_bookmark_save(Bookmark *bookmark)
{
  BufferedServerBookmarkData *bookmark_data = (BufferedServerBookmarkData *)(&bookmark->container);
  LogProtoBufferedServerState *state = persist_state_map_entry(bookmark->persist_state, bookmark_data->persist_handle);

  state->buffer_pos = bookmark_data->pending_buffer_pos;
  state->raw_stream_pos = bookmark_data->pending_raw_stream_pos;
  state->raw_buffer_size = bookmark_data->pending_raw_buffer_size;

  msg_trace("Last message got confirmed",
            evt_tag_int("raw_stream_pos", state->raw_stream_pos),
            evt_tag_int("raw_buffer_len", state->raw_buffer_size),
            evt_tag_int("buffer_pos", state->buffer_pos),
            evt_tag_int("buffer_end", state->pending_buffer_end));
  persist_state_unmap_entry(bookmark->persist_state, bookmark_data->persist_handle);
}

static void
_buffered_server_bookmark_fill(LogProtoBufferedServer *self, Bookmark *bookmark)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(self);
  BufferedServerBookmarkData *data = (BufferedServerBookmarkData *)(&bookmark->container);

  data->pending_buffer_pos = state->pending_buffer_pos;
  data->pending_raw_stream_pos = state->pending_raw_stream_pos;
  data->pending_raw_buffer_size = state->pending_raw_buffer_size;
  data->persist_handle = self->persist_handle;
  bookmark->save = _buffered_server_bookmark_save;

  log_proto_buffered_server_put_state(self);
}

/**
 * Returns: TRUE to indicate success, FALSE otherwise. The returned
 * msg can be NULL even if no failure occurred.
 **/
static LogProtoStatus
log_proto_buffered_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                                LogTransportAuxData *aux, Bookmark *bookmark)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;
  LogProtoStatus result = LPS_SUCCESS;

  while (1)
    {
      if (self->fetch_state == LPBSF_FETCHING_FROM_BUFFER)
        {
          if (log_proto_buffered_server_fetch_from_buffer(self, msg, msg_len, aux))
            goto exit;

          if (log_proto_buffered_server_is_input_closed(self))
            {
              result = _convert_io_status_to_log_proto_status(self->io_status);
              goto exit;
            }
          else
            {
              self->fetch_state = LPBSF_FETCHING_FROM_INPUT;
            }
        }
      else if (self->fetch_state == LPBSF_FETCHING_FROM_INPUT)
        {
          GIOStatus io_status;

          if (!(*may_read))
            goto exit;

          io_status = log_proto_buffered_server_fetch_into_buffer(self);
          switch (io_status)
            {
            case G_IO_STATUS_NORMAL:
              if (self->no_multi_read)
                *may_read = FALSE;
              break;

            case G_IO_STATUS_AGAIN:
              goto exit;

            case G_IO_STATUS_ERROR:
            case G_IO_STATUS_EOF:
              self->io_status = io_status;
              break;
            default:
              g_assert_not_reached();
            }
          self->fetch_state = LPBSF_FETCHING_FROM_BUFFER;
        }
    }

exit:

  /* result contains our result, but once an error happens, the error condition remains persistent */
  if (result != LPS_SUCCESS)
    self->super.status = result;
  else
    {
      if (bookmark && *msg)
        {
          _buffered_server_bookmark_fill(self, bookmark);
          _buffered_server_update_pos(&self->super);
        }
    }
  return result;
}

static gboolean
log_proto_buffered_server_is_position_tracked(LogProtoServer *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;

  return self->pos_tracking;
}

gboolean
log_proto_buffered_server_validate_options_method(LogProtoServer *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;

  if (self->super.options->encoding && self->convert == (GIConv) -1)
    {
      msg_error("Unknown character set name specified",
                evt_tag_str("encoding", self->super.options->encoding));
      return FALSE;
    }
  return log_proto_server_validate_options_method(s);
}

void
log_proto_buffered_server_free_method(LogProtoServer *s)
{
  LogProtoBufferedServer *self = (LogProtoBufferedServer *) s;

  log_transport_aux_data_destroy(&self->buffer_aux);

  g_free(self->buffer);
  if (self->state1)
    {
      g_free(self->state1);
    }
  if (self->convert != (GIConv) -1)
    g_iconv_close(self->convert);
  log_proto_server_free_method(s);
}

void
log_proto_buffered_server_init(LogProtoBufferedServer *self, LogTransport *transport,
                               const LogProtoServerOptions *options)
{
  log_proto_server_init(&self->super, transport, options);
  self->super.prepare = log_proto_buffered_server_prepare;
  self->super.fetch = log_proto_buffered_server_fetch;
  self->super.free_fn = log_proto_buffered_server_free_method;
  self->super.transport = transport;
  self->super.restart_with_state = log_proto_buffered_server_restart_with_state;
  self->super.is_position_tracked = log_proto_buffered_server_is_position_tracked;
  self->super.validate_options = log_proto_buffered_server_validate_options_method;
  self->convert = (GIConv) -1;
  self->read_data = log_proto_buffered_server_read_data_method;
  self->io_status = G_IO_STATUS_NORMAL;
  if (options->encoding)
    self->convert = g_iconv_open("utf-8", options->encoding);
  else
    self->convert = (GIConv) -1;
  self->stream_based = TRUE;
  self->pos_tracking = options->position_tracking_enabled;
}
