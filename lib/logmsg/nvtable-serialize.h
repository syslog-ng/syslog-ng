/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2012-2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "logmsg/nvtable.h"
#include "logmsg/serialization.h"

#define NV_TABLE_MAGIC_V2  "NVT2"
#define NVT_SF_BE           0x1
#define NVT_SUPPORTS_UNSET  0x2

#define NVENTRY_FLAGS_DEFINED_IN_LEGACY_FORMATS 0x3

NVTable *nv_table_deserialize(LogMessageSerializationState *state);
gboolean nv_table_serialize(LogMessageSerializationState *state, NVTable *self);
gboolean nv_table_fixup_handles(LogMessageSerializationState *state);

#endif
