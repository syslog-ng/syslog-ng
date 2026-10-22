/*
 * Copyright (c) 2019-2025 Airbus Commercial Aircraft
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

#ifndef UTILS_SLOG_H_INCLUDED
#define UTILS_SLOG_H_INCLUDED 1

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

// Argument indicators for command line utilities
#define LONG_OPT_INDICATOR "--"
#define SHORT_OPT_INDICATOR "-"

enum LogMode
{
  LOGMODE_INVALID,      //-- uninitialized, ERROR
  LOGMODE_PLAIN_DIRECT, //-- direct, do NOT encrypt and do NOT Base64 encode log message
  LOGMODE_PLAIN_BASE64, //-- base64, do NOT encrypt and do Base64 encode log message
  LOGMODE_ENCRYPTED,    //-- enc, do encrypt and do Base64 encode log message
};

gboolean validLogModeArg(const gchar *option_name, const gchar *value, gpointer data, GError **error);

enum LogMode convert_str_logmode(const gchar *value);
GString *convert_logmode_str(enum LogMode val);

void truncate_utf8_gstring(GString *gslog, gsize max_octet_len);
gboolean is_likely_base64(const gchar *str);
gsize get_base64_length( gsize uncoded_count );
gsize get_decoded_base64_length(const char *encoded_str);

void dbg_hexdump(const char *title, const unsigned char *data, unsigned int len);

//-- helper to check file path argument
gboolean is_file_path_safe_and_valid(const gchar *input_path);

#endif /* UTILS_SLOG_H_INCLUDED */

