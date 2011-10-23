/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef MSG_FORMAT_H_INCLUDED
#define MSG_FORMAT_H_INCLUDED

#include "syslog-ng.h"
#include "timeutils.h"
#include "logproto.h"

#include <regex.h>

enum
{
  /* don't parse the message, put everything into $MSG */
  LP_NOPARSE         = 0x0001,
  /* check if the hostname contains valid characters and assume it is part of the program field if it isn't */
  LP_CHECK_HOSTNAME  = 0x0002,
  /* message is using RFC5424 format */
  LP_SYSLOG_PROTOCOL = 0x0004,
  /* the caller knows the message is valid UTF-8 */
  LP_ASSUME_UTF8     = 0x0008,
  /* validate that all characters are indeed UTF-8 and mark the message as valid when relaying */
  LP_VALIDATE_UTF8   = 0x0010,
  /* the message may not contain NL characters, strip them if it does */
  LP_NO_MULTI_LINE   = 0x0020,
  /* don't store MSGHDR in the LEGACY_MSGHDR macro */
  LP_STORE_LEGACY_MSGHDR = 0x0040,
  /* expect a hostname field in the message */
  LP_EXPECT_HOSTNAME = 0x0080,
  /* message is locally generated and should be marked with LF_LOCAL */
  LP_LOCAL = 0x0100,
};

typedef struct _MsgFormatHandler MsgFormatHandler;

typedef struct _MsgFormatOptions
{
  gchar *format;
  MsgFormatHandler *format_handler;
  guint32 flags;
  guint16 default_pri;
  gchar *recv_time_zone;
  TimeZoneInfo *recv_time_zone_info;
  regex_t *bad_hostname;
} MsgFormatOptions;

struct _MsgFormatHandler
{
  LogProto *(*construct_proto)(MsgFormatOptions *options, LogTransport *transport, guint flags);
  void (*parse)(MsgFormatOptions *options, const guchar *data, gsize length, LogMessage *msg);
};

void msg_format_options_defaults(MsgFormatOptions *options);
void msg_format_options_init(MsgFormatOptions *parse_options, GlobalConfig *cfg);
void msg_format_options_destroy(MsgFormatOptions *parse_options);


#endif
