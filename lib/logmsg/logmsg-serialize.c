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
#include "logmsg/logmsg.h"
#include "logmsg/nvtable-serialize.h"
#include "logmsg/gsockaddr-serialize.h"
#include "logmsg/timestamp-serialize.h"
#include "logmsg/tags-serialize.h"
#include "gsockaddr.h"
#include "serialize.h"
#include "messages.h"

#include <stdlib.h>

gboolean
log_msg_serialize(LogMessage *self, SerializeArchive *sa)
{
  guint8 version = 26;
  gint i = 0;

  serialize_write_uint8(sa, version);
  serialize_write_uint64(sa, self->rcptid);
  g_assert(sizeof(self->flags) == 4);
  serialize_write_uint32(sa, self->flags & ~LF_STATE_MASK);
  serialize_write_uint16(sa, self->pri);
  g_sockaddr_serialize(sa, self->saddr);
  timestamp_serialize(sa, self->timestamps);
  serialize_write_uint32(sa, self->host_id);
  tags_serialize(self, sa);
  serialize_write_uint8(sa, self->initial_parse);
  serialize_write_uint8(sa, self->num_matches);
  serialize_write_uint8(sa, self->num_sdata);
  serialize_write_uint8(sa, self->alloc_sdata);
  for (i = 0; i < self->num_sdata; i++)
    serialize_write_uint32(sa, self->sdata[i]);
  nv_table_serialize(sa, self->payload);
  return TRUE;
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
_deserialize_message(LogMessage *self, SerializeArchive *sa)
{
  guint8 initial_parse = 0;

  if (!serialize_read_uint64(sa, &self->rcptid))
     return FALSE;
  if (!serialize_read_uint32(sa, &self->flags))
     return FALSE;
  self->flags |= LF_STATE_MASK;
  if (!serialize_read_uint16(sa, &self->pri))
     return FALSE;
  if (!g_sockaddr_deserialize(sa, &self->saddr))
     return FALSE;
  if (!timestamp_deserialize(sa, self->timestamps))
    return FALSE;
  if (!serialize_read_uint32(sa, &self->host_id))
    return FALSE;

  if (!tags_deserialize(self, sa))
    return FALSE;

  if (!serialize_read_uint8(sa, &initial_parse))
    return FALSE;
  self->initial_parse=initial_parse;

  if (!serialize_read_uint8(sa, &self->num_matches))
    return FALSE;

  if (!_deserialize_sdata(self, sa))
    return FALSE;

  nv_table_unref(self->payload);
  self->payload = nv_table_deserialize(sa);
  if(!self->payload)
    return FALSE;

  nv_table_update_handles(self->payload, logmsg_registry, self->sdata, self->num_sdata);
  return TRUE;
}

static gboolean
_check_msg_version(SerializeArchive *sa)
{
  guint8 version;

  if (!serialize_read_uint8(sa, &version))
    return FALSE;
  if (version != 26)
    {
      msg_error("Error deserializing log message, unsupported version",
          evt_tag_int("version", version));
      return FALSE;
    }
  return TRUE;
}

gboolean
log_msg_deserialize(LogMessage *self, SerializeArchive *sa)
{
  if (!_check_msg_version(sa))
    {
      return FALSE;
    }
  return _deserialize_message(self, sa);
}
