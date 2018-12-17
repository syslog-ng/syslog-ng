/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef STR_FORMAT_H_INCLUDED
#define STR_FORMAT_H_INCLUDED

#include "syslog-ng.h"

gint format_uint32_padded(GString *result, gint field_len, gchar pad_char, gint base, guint32 value);
gint format_int32_padded(GString *result, gint field_len, gchar pad_char, gint base, gint32 value);

gint format_uint64_padded(GString *result, gint field_len, gchar pad_char, gint base, guint64 value);
gint format_int64_padded(GString *result, gint field_len, gchar pad_char, gint base, gint64 value);

gchar *format_hex_string(gpointer str, gsize str_len, gchar *result, gsize result_len);
gchar *format_hex_string_with_delimiter(gpointer str, gsize str_len, gchar *result, gsize result_len, gchar delimiter);


gboolean scan_int(const gchar **buf, gint *left, gint field_width, gint *num);
gboolean scan_expect_char(const gchar **buf, gint *left, gchar value);
gboolean scan_expect_str(const gchar **buf, gint *left, const gchar *value);

#endif
