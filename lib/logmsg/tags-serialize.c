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

#include "tags-serialize.h"
#include "scratch-buffers.h"

gboolean
tags_deserialize(LogMessage *msg, SerializeArchive *sa)
{
  ScratchBuffersMarker marker;
  GString *buf = scratch_buffers_alloc_and_mark(&marker);

  while (1)
    {
      if (!serialize_read_string(sa, buf))
        return FALSE;
      if (buf->len == 0)
        {
          /* "" , empty string means: last tag */
          break;
        }
      log_msg_set_tag_by_name(msg, buf->str);
    }

  msg->flags |= LF_STATE_OWN_TAGS;

  scratch_buffers_reclaim_marked(marker);
  return TRUE;
}

static gboolean
_callback(const LogMessage *msg, LogTagId tag_id, const gchar *name, gpointer user_data)
{
  SerializeArchive *sa = ( SerializeArchive *)user_data;
  serialize_write_cstring(sa, name, strlen(name));
  return TRUE;
}

gboolean
tags_serialize(LogMessage *msg, SerializeArchive *sa)
{
  log_msg_tags_foreach(msg, _callback, (gpointer)sa);
  return serialize_write_cstring(sa, "", 0);
}


