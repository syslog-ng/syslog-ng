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
#include <zlib.h>

gint8 CURL_COMPRESSION_DEFAULT = CURL_COMPRESSION_UNCOMPRESSED;
gchar *CURL_COMPRESSION_LITERAL_ALL = "all";
gchar *curl_compression_types[] = {"identity", "gzip", "deflate"};

enum _DeflateAlgorithmTypes
{
  DEFLATE_TYPE_DEFLATE,
  DEFLATE_TYPE_GZIP

};

gboolean
http_dd_curl_compression_string_match(const gchar *string, gint curl_compression_index)
{
  return (strcmp(string, curl_compression_types[curl_compression_index]) == 0);
}

gboolean
http_dd_check_curl_compression(const gchar *type)
{
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_UNCOMPRESSED)) return TRUE;
#ifdef SYSLOG_NG_HAVE_ZLIB
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_GZIP)) return TRUE;
#endif
#ifdef SYSLOG_NG_HAVE_ZLIB
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_DEFLATE)) return TRUE;
#endif
  return FALSE;
}

gchar *_compression_error_message = "Failed due to %s error.";
static inline void _handle_compression_error(GString *compression_dest, gchar *error_description)
{
}
static inline gboolean
_raise_compression_status(GString *compression_dest, int algorithm_exit)
{
  return FALSE;
}

int
_gzip_string(GString *compressed, const GString *message);
int
_deflate_string(GString *compressed, const GString *message);

int
_deflate_type_compression(GString *compressed, const GString *message, const gint deflate_algorithm_type);

gboolean
http_dd_compress_string(GString *compression_destination, const GString *message, const gint compression)
{
  int err_compr;
  switch (compression)
    {
    case CURL_COMPRESSION_GZIP:
      err_compr = _gzip_string(compression_destination, message);
      break;
    case CURL_COMPRESSION_DEFLATE:
      err_compr = _deflate_string(compression_destination, message);
      break;
    default:
      g_assert_not_reached();
    }
  return _raise_compression_status(compression_destination, err_compr);
}

int
_gzip_string(GString *compressed, const GString *message)
{
  return _deflate_type_compression(compressed, message, DEFLATE_TYPE_GZIP);
}

int
_deflate_string(GString *compressed, const GString *message)
{
  return _deflate_type_compression(compressed, message, DEFLATE_TYPE_DEFLATE);
}

int
_deflate_type_compression(GString *compressed, const GString *message, const gint deflate_algorithm_type)
{
  //not yet implemented
  g_assert_not_reached();
}
