/*
 * Copyright (c) 2012-2013 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef LIBTEST_TEMPLATE_LIB_H_INCLUDED
#define LIBTEST_TEMPLATE_LIB_H_INCLUDED 1

#include "testutils.h"
#include "templates.h"

void assert_template_format(const gchar *template, const gchar *expected);
void assert_template_format_with_escaping(const gchar *template, gboolean escaping, const gchar *expected);
void assert_template_format_with_context(const gchar *template, const gchar *expected);
void assert_template_failure(const gchar *template, const gchar *expected_failure);

LogMessage *create_sample_message(void);
LogTemplate *compile_template(const gchar *template, gboolean escaping);
void init_template_tests(void);
void deinit_template_tests(void);


#endif
