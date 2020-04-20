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


#ifndef LOGMSG_SERIALIZE_H
#define LOGMSG_SERIALIZE_H

#include "serialize.h"
#include "timeutils/unixtime.h"

/*
 * version   info
 *   0       first introduced in syslog-ng-2.1
 *   1       dropped self->date
 *   10      added support for values,
 *           sd data,
 *           syslog-protocol fields,
 *           timestamps became 64 bits,
 *           removed source group string
 *           flags & pri became 16 bits
 *   11      flags became 32 bits
 *   12      store tags
 *   20      usage of the nvtable
 *   21      sdata serialization
 *   22      corrected nvtable serialization
 *   23      new RCTPID field (64 bits)
 *   24      new processed timestamp
 *   25      added hostid
 *   26      use 32 bit values nvtable
 */

enum _LogMessageVersion
{
  LGM_V01 = 1,
  LGM_V10 = 10,
  LGM_V11 = 11,
  LGM_V12 = 12,
  LGM_V20 = 20,
  LGM_V21 = 21,
  LGM_V22 = 22,
  LGM_V23 = 23,
  LGM_V24 = 24,
  LGM_V25 = 25,
  LGM_V26 = 26
};

enum _LogMessageSerializationFlags
{
  LMSF_COMPACTION = 0x0001,
};

gboolean log_msg_deserialize(LogMessage *self, SerializeArchive *sa);
gboolean log_msg_serialize_with_ts_processed(LogMessage *self, SerializeArchive *sa, const UnixTime *processed,
                                             guint32 flags);
gboolean log_msg_serialize(LogMessage *self, SerializeArchive *sa, guint32 flags);

#endif
