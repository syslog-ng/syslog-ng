/*
 * Copyright (c) 2023 One Identity LLC.
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

#ifndef LOGPROTO_FRAMED_SERVER_PRIVATE_H_INCLUDED
#define LOGPROTO_FRAMED_SERVER_PRIVATE_H_INCLUDED

#include "logproto-framed-server.h"
#include "messages.h"

#include <errno.h>
#include <ctype.h>
#include <string.h>

#define MAX_FRAME_LEN_DIGITS 10
static const guint MAX_FETCH_COUNT = 3;

typedef enum
{
  LPFSS_FRAME_READ,
  LPFSS_FRAME_EXTRACT,
  LPFSS_MESSAGE_READ,
  LPFSS_MESSAGE_EXTRACT,
  LPFSS_TRIM_MESSAGE,
  LPFSS_TRIM_MESSAGE_READ,
  LPFSS_CONSUME_TRIMMED
} LogProtoFramedServerState;

typedef enum
{
  LPFSSCTRL_RETURN_WITH_STATUS,
  LPFSSCTRL_NEXT_STATE,
} LogProtoFramedServerStateControl;


typedef struct _LogProtoFramedServer
{
  LogProtoServer super;
  LogProtoFramedServerState state;

  guchar *buffer;
  guint32 buffer_size, buffer_pos, buffer_end;
  guint32 frame_len;
  gboolean half_message_in_buffer;
  guint32 fetch_counter;
} LogProtoFramedServer;

LogProtoFramedServerStateControl
_step_state_machine(LogProtoFramedServer *self, const guchar **msg, gsize *msg_len, gboolean *may_read,
                    LogTransportAuxData *aux, LogProtoStatus *status);

#endif
