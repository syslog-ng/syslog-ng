/*
 * Copyright (c) 2015-2016 Balabit
 * Copyright (c) 2015-2016 Balázs Scheidler
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
#ifndef SYSLOG_NG_C_LITERAL_UNESCAPE_H_INCLUDED
#define SYSLOG_NG_C_LITERAL_UNESCAPE_H_INCLUDED

#include "syslog-ng.h"

typedef gboolean (*MatchDelimiterFunc)(const gchar *cur, const gchar **new_cur, gpointer user_data);

typedef struct _StrReprDecodeOptions
{
  MatchDelimiterFunc match_delimiter;
  gpointer match_delimiter_data;
  gchar delimiter_chars[3];
} StrReprDecodeOptions;

gboolean str_repr_decode(GString *value, const gchar *input, const gchar **end);
gboolean str_repr_decode_append(GString *value, const gchar *input, const gchar **end);

gboolean str_repr_decode_with_options(GString *value, const gchar *input, const gchar **end,
                                      const StrReprDecodeOptions *options);
gboolean str_repr_decode_append_with_options(GString *value, const gchar *input, const gchar **end,
                                             const StrReprDecodeOptions *options);

#endif
