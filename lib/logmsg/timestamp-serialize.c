/*
 * Copyright (c) 2002-2015 Balabit
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

#include "timestamp-serialize.h"

static gboolean
_write_log_stamp(SerializeArchive *sa, const LogStamp *stamp)
{
  return serialize_write_uint64(sa, stamp->tv_sec) &&
         serialize_write_uint32(sa, stamp->tv_usec) &&
         serialize_write_uint32(sa, stamp->zone_offset);
}

static gboolean
_read_log_stamp(SerializeArchive *sa, LogStamp *stamp)
{
  guint64 val64;
  guint32 val;

  if (!serialize_read_uint64(sa, &val64))
    return FALSE;
  stamp->tv_sec = (gint64) val64;

  if (!serialize_read_uint32(sa, &val))
    return FALSE;
  stamp->tv_usec = val;

  if (!serialize_read_uint32(sa, &val))
    return FALSE;
  stamp->zone_offset = (gint) val;
  return TRUE;
}


gboolean
timestamp_serialize(SerializeArchive *sa, LogStamp *timestamps)
{
  return _write_log_stamp(sa, &timestamps[LM_TS_STAMP]) &&
         _write_log_stamp(sa, &timestamps[LM_TS_RECVD]) &&
         _write_log_stamp(sa, &timestamps[LM_TS_PROCESSED]);
}

gboolean
timestamp_deserialize_legacy(SerializeArchive *sa, LogStamp *timestamps)
{
  return (_read_log_stamp(sa, &timestamps[LM_TS_STAMP]) &&
          _read_log_stamp(sa, &timestamps[LM_TS_RECVD]));
}

gboolean
timestamp_deserialize(SerializeArchive *sa, LogStamp *timestamps)
{
  return (timestamp_deserialize_legacy(sa, timestamps) &&
          _read_log_stamp(sa, &timestamps[LM_TS_PROCESSED]));
}
