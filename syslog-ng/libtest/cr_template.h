/*
 * Copyright (c) 2012-2018 Balabit
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

#ifndef LIBTEST_CR_TEMPLATE_H_INCLUDED
#define LIBTEST_CR_TEMPLATE_H_INCLUDED 1

#include "syslog-ng.h"
#include "template/templates.h"
#include "msg-format.h"
#include <stdarg.h>

void assert_template_format(const gchar *template, const gchar *expected);
void assert_template_format_msg(const gchar *template,
                                const gchar *expected, LogMessage *msg);
void assert_template_format_with_escaping(const gchar *template, gboolean escaping, const gchar *expected);
void assert_template_format_with_escaping_msg(const gchar *template, gboolean escaping,
                                              const gchar *expected, LogMessage *msg);
void assert_template_format_with_context(const gchar *template, const gchar *expected);
void assert_template_format_with_context_msgs(const gchar *template, const gchar *expected, LogMessage **msgs,
                                              gint num_messages);
void assert_template_format_with_len(const gchar *template, const gchar *expected, gssize expected_len);
void assert_template_format_with_escaping_and_context_msgs(const gchar *template, gboolean escaping,
                                                           const gchar *expected, gssize expected_len,
                                                           LogMessage **msgs, gint num_messages);
void assert_template_failure(const gchar *template, const gchar *expected_failure);
void perftest_template(gchar *template);

LogMessage *create_empty_message(void);
LogMessage *create_sample_message(void);
LogMessage *message_from_list(va_list ap);
LogTemplate *compile_template(const gchar *template, gboolean escaping);
void init_template_tests(void);
void deinit_template_tests(void);


#endif
