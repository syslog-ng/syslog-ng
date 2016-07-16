/*
 * Copyright (c) 2010-2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "logmsg/logmsg-serialize.h"
#include "logmsg/serialization.h"
#include "logmsg/logmsg.h"
#include "logmsg/nvtable-serialize.h"
#include "logmsg/gsockaddr-serialize.h"
#include "logmsg/timestamp-serialize.h"
#include "logmsg/tags-serialize.h"
#include "gsockaddr.h"
#include "serialize.h"
#include "messages.h"

#include <stdlib.h>

static gboolean
_serialize_message(LogMessageSerializationState *state)
{
  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;
  gint i = 0;

  serialize_write_uint8(sa, state->version);
  serialize_write_uint64(sa, msg->rcptid);
  g_assert(sizeof(msg->flags) == 4);
  serialize_write_uint32(sa, msg->flags & ~LF_STATE_MASK);
  serialize_write_uint16(sa, msg->pri);
  g_sockaddr_serialize(sa, msg->saddr);
  timestamp_serialize(sa, msg->timestamps);
  serialize_write_uint32(sa, msg->host_id);
  tags_serialize(msg, sa);
  serialize_write_uint8(sa, msg->initial_parse);
  serialize_write_uint8(sa, msg->num_matches);
  serialize_write_uint8(sa, msg->num_sdata);
  serialize_write_uint8(sa, msg->alloc_sdata);
  for (i = 0; i < msg->num_sdata; i++)
    serialize_write_uint32(sa, msg->sdata[i]);
  nv_table_serialize(state, msg->payload);
  return TRUE;
}

gboolean
log_msg_serialize(LogMessage *self, SerializeArchive *sa)
{
  LogMessageSerializationState state = { 0 };

  state.version = 26;
  state.msg = self;
  state.sa = sa;
  return _serialize_message(&state);
}

static gboolean
_deserialize_sdata(LogMessage *self, SerializeArchive *sa)
{
  gint i;

  if (!serialize_read_uint8(sa, &self->num_sdata))
      return FALSE;

    if (!serialize_read_uint8(sa, &self->alloc_sdata))
      return FALSE;

  self->sdata = (NVHandle*)g_malloc(sizeof(NVHandle)*self->alloc_sdata);
  memset(self->sdata, 0, sizeof(NVHandle)*self->alloc_sdata);

  for (i = 0; i < self->num_sdata; i++)
    {
      if (!serialize_read_uint32(sa, (guint32 *)(&self->sdata[i])))
        return FALSE;
    }
  return TRUE;
}

static gboolean
_deserialize_message(LogMessageSerializationState *state)
{
  guint8 initial_parse = 0;
  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;

  if (!serialize_read_uint64(sa, &msg->rcptid))
     return FALSE;
  if (!serialize_read_uint32(sa, &msg->flags))
     return FALSE;
  msg->flags |= LF_STATE_MASK;
  if (!serialize_read_uint16(sa, &msg->pri))
     return FALSE;
  if (!g_sockaddr_deserialize(sa, &msg->saddr))
     return FALSE;
  if (!timestamp_deserialize(sa, msg->timestamps))
    return FALSE;
  if (!serialize_read_uint32(sa, &msg->host_id))
    return FALSE;

  if (!tags_deserialize(msg, sa))
    return FALSE;

  if (!serialize_read_uint8(sa, &initial_parse))
    return FALSE;
  msg->initial_parse=initial_parse;

  if (!serialize_read_uint8(sa, &msg->num_matches))
    return FALSE;

  if (!_deserialize_sdata(msg, sa))
    return FALSE;

  nv_table_unref(msg->payload);
  msg->payload = nv_table_deserialize(state);
  if (!msg->payload)
    return FALSE;

  if (!nv_table_fixup_handles(state))
    return FALSE;
  return TRUE;
}

static gboolean
_check_msg_version(LogMessageSerializationState *state)
{
  if (!serialize_read_uint8(state->sa, &state->version))
    return FALSE;

  if (state->version != 26)
    {
      msg_error("Error deserializing log message, unsupported version, "
                "we only support v26 introduced in " VERSION_3_8 ", "
                "earlier versions in syslog-ng Premium Editions are not supported",
                evt_tag_int("version", state->version));
      return FALSE;
    }
  return TRUE;
}

gboolean
log_msg_deserialize(LogMessage *self, SerializeArchive *sa)
{
  LogMessageSerializationState state = { 0 };

  state.sa = sa;
  state.msg = self;
  if (!_check_msg_version(&state))
    {
      return FALSE;
    }
  return _deserialize_message(&state);
}
