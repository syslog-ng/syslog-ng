/*
 * Copyright (c) 2022 One Identity LLC.
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

#ifndef SYSLOG_NG_COMPRESSION_H
#define SYSLOG_NG_COMPRESSION_H

#include <glib.h>

enum CurlCompressionTypes
{
  CURL_COMPRESSION_UNCOMPRESSED = 0,
  CURL_COMPRESSION_GZIP = 1,
  CURL_COMPRESSION_DEFLATE = 2
};

extern gint8 CURL_COMPRESSION_DEFAULT;
extern gchar *CURL_COMPRESSION_LITERAL_ALL;

extern gchar *curl_compression_types[];
gboolean http_dd_compress_string(GString *compression_destination, const GString *message, const gint compression);
gboolean http_dd_curl_compression_string_match(const gchar *string, gint curl_compression_index);
gboolean http_dd_check_curl_compression(const gchar *type);

typedef struct Compressor Compressor;

Compressor get_gzip_compressor(void);

Compressor get_deflate_compressor(void);

#endif //SYSLOG_NG_COMPRESSION_H
