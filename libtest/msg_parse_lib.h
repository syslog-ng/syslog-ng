/*
 * Copyright (c) 2012-2019 Balabit
 * Copyright (c) 2012 Balázs Scheidler
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

#ifndef MSG_PARSE_LIB_H_INCLUDED
#define MSG_PARSE_LIB_H_INCLUDED

#include "testutils.h"

#include "cfg.h"
#include "logmsg/logmsg.h"

extern MsgFormatOptions parse_options;

void init_and_load_syslogformat_module(void);
void deinit_syslogformat_module(void);

void assert_log_messages_equal(LogMessage *log_message_a, LogMessage *log_message_b);

void assert_log_message_value(LogMessage *self, NVHandle handle, const gchar *expected_value);
void assert_log_message_value_by_name(LogMessage *self, const gchar *name, const gchar *expected_value);
void assert_log_message_has_tag(LogMessage *log_message, const gchar *tag_name);
void assert_log_message_doesnt_have_tag(LogMessage *log_message, const gchar *tag_name);
void assert_log_messages_saddr(LogMessage *log_message_a, LogMessage *log_message_b);
void assert_structured_data_of_messages(LogMessage *log_message_a, LogMessage *log_message_b);
void assert_log_message_values_equal(LogMessage *log_message_a, LogMessage *log_message_b, NVHandle handle);

#endif
