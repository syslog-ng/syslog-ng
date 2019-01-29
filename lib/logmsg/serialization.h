/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Balazs Scheidler <balazs.scheidler@balabit.com>
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


#ifndef LOGMSG_SERIALIZATION_H
#define LOGMSG_SERIALIZATION_H

#include "serialize.h"
#include "logmsg/logmsg.h"
#include "timeutils/unixtime.h"

typedef struct _LogMessageSerializationState
{
  guint8 version;
  SerializeArchive *sa;
  LogMessage *msg;
  NVTable *nvtable;
  guint8 nvtable_flags;
  guint8 handle_changed;
  NVHandle *updated_sdata_handles;
  NVIndexEntry *updated_index;
  const UnixTime *processed;
} LogMessageSerializationState;

#endif
