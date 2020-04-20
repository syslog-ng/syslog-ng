/*
 * Copyright (c) 2005-2018 Balabit
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

#ifndef __TEST_FILTER_COMMON_H__
#define __TEST_FILTER_COMMON_H__
void
testcase_with_socket(const gchar *msg, const gchar *sockaddr,
                     FilterExprNode *f,
                     gboolean expected_result);

void
testcase(const gchar *msg,
         FilterExprNode *f,
         gboolean expected_result);

void
testcase_with_backref_chk(const gchar *msg,
                          FilterExprNode *f,
                          gboolean expected_result,
                          const gchar *name,
                          const gchar *value
                         );

FilterExprNode *create_pcre_regexp_filter(gint field, const gchar *regexp, gint flags);
FilterExprNode *create_pcre_regexp_match(const gchar *regexp, gint flags);
LogTemplate *create_template(const gchar *template);
FilterExprNode *compile_pattern(FilterExprNode *f, const gchar *regexp, const gchar *type, gint flags);

gint facility_bits(const gchar *fac);
gint level_bits(const gchar *lev);
gint level_range(const gchar *from, const gchar *to);

void setup(void);

void teardown(void);

#endif
