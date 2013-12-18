/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2012-2013 Viktor Juhasz
 * Copyright (c) 2012-2013 Viktor Tusa
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

#ifndef _NVTABLE_SERIALIZE_H
#define _NVTABLE_SERIALIZE_H

#include "nvtable.h"
#include "serialize.h"

NVTable *nv_table_deserialize(SerializeArchive *sa, guint8 log_msg_version);
gboolean nv_table_serialize(SerializeArchive *sa, NVTable *self);
void nv_table_update_ids(NVTable *self, NVRegistry *logmsg_registry,
                         NVHandle *handles_to_update, guint8 num_handles_to_update);

static inline gboolean
serialize_read_nvhandle(SerializeArchive *sa, NVHandle* handle)
{
  return serialize_read_uint32(sa, handle);
}

static inline gboolean
serialize_write_nvhandle(SerializeArchive *sa, NVHandle handle)
{
  return serialize_write_uint32(sa, handle);
}

#endif
