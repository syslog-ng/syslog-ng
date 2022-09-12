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

#include "compression.h"

gint8 CURL_COMPRESSION_DEFAULT = CURL_COMPRESSION_UNCOMPRESSED;
gchar *CURL_COMPRESSION_LITERAL_ALL = "all";
gchar *curl_compression_types[] = {"identity", "gzip", "deflate"};

gboolean
http_dd_curl_compression_string_match(const gchar *string, gint curl_compression_index)
{
  return (strcmp(string, curl_compression_types[curl_compression_index]) == 0);
}

gboolean
http_dd_check_curl_compression(const gchar *type)
{
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_UNCOMPRESSED)) return TRUE;
#ifdef ZLIB_AVAILABLE
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_GZIP)) return TRUE;
#endif
#ifdef ZLIB_AVAILABLE
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_DEFLATE)) return TRUE;
#endif
  return FALSE;
}

gboolean
http_dd_compress_string(GString *compression_destination, const GString *message, const gint compression)
{
  return FALSE;
}