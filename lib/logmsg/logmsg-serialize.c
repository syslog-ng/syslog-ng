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
#include "logmsg/logmsg-serialize-fixup.h"
#include "logmsg/nvtable-serialize.h"
#include "logmsg/gsockaddr-serialize.h"
#include "logmsg/timestamp-serialize.h"
#include "logmsg/tags-serialize.h"
#include "messages.h"

#include <stdlib.h>

static gboolean
_serialize_message(LogMessageSerializationState *state)
{
  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;

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
  serialize_write_uint32_array(sa, (guint32 *) msg->sdata, msg->num_sdata);
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
_deserialize_sdata(LogMessageSerializationState *state)
{
  LogMessage *self = state->msg;
  SerializeArchive *sa = state->sa;

  if (!serialize_read_uint8(sa, &self->num_sdata))
    return FALSE;

  if (!serialize_read_uint8(sa, &self->alloc_sdata))
    return FALSE;

  g_assert(!self->sdata);
  self->sdata = (NVHandle *) g_malloc(sizeof(NVHandle)*self->alloc_sdata);

  if (state->version <= 20)
    return TRUE;

  if ((state->version < 26) && !serialize_read_uint16_array(sa, (guint32 *) self->sdata, self->num_sdata))
    return FALSE;

  if ((state->version == 26) && !serialize_read_uint32_array(sa, (guint32 *) self->sdata, self->num_sdata))
    return FALSE;

  return TRUE;
}

static gboolean
_deserialize_message_version_2x(LogMessageSerializationState *state)
{
  g_assert(state->version >= 20);

  guint8 initial_parse = 0;
  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;

  if ((state->version > 22) && !serialize_read_uint64(sa, &msg->rcptid))
    return FALSE;
  if (!serialize_read_uint32(sa, &msg->flags))
    return FALSE;
  msg->flags |= LF_STATE_MASK;
  if (!serialize_read_uint16(sa, &msg->pri))
    return FALSE;
  if (!g_sockaddr_deserialize(sa, &msg->saddr))
    return FALSE;
  if (!timestamp_deserialize(state->version, sa, msg->timestamps))
    return FALSE;
  if ((state->version >= 25) && (!serialize_read_uint32(sa, &msg->host_id)))
    return FALSE;

  if (!tags_deserialize(msg, sa))
    return FALSE;

  if (!serialize_read_uint8(sa, &initial_parse))
    return FALSE;
  msg->initial_parse=initial_parse;

  if (!serialize_read_uint8(sa, &msg->num_matches))
    return FALSE;

  if (!_deserialize_sdata(state))
    return FALSE;

  nv_table_unref(msg->payload);
  msg->payload = nv_table_deserialize(state);
  if (!msg->payload)
    return FALSE;

  if (!log_msg_fixup_handles_after_deserialization(state))
    return FALSE;
  return TRUE;
}

static gboolean
_deserialize_message_version_0_1(LogMessageSerializationState *state)
{
  //TODO: not yet implemented
  g_assert_not_reached();
  return FALSE;
}

static gboolean
_deserialize_message_version_1x(LogMessageSerializationState *state)
{
  //TODO: not yet implemented
  g_assert_not_reached();
  return FALSE;
}

static gboolean
_check_msg_version(LogMessageSerializationState *state)
{
  if (!serialize_read_uint8(state->sa, &state->version))
    return FALSE;

  if ((state->version > 1 && state->version < 10) || state->version > 26)
    {
      msg_error("Error deserializing log message, unsupported version, ",
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

  if (state.version < 10)
    return _deserialize_message_version_0_1(&state);
  else if (state.version < 20)
    return _deserialize_message_version_1x(&state);
  else if (state.version <= 26)
    return _deserialize_message_version_2x(&state);

  g_assert_not_reached();
  return FALSE;
}
