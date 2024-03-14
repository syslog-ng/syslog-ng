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

#include "syslog-ng.h"
#include "compat/curl.h"

#if defined(SYSLOG_NG_HAVE_ZLIB) && defined(CURL_VERSION_LIBZ)
#define SYSLOG_NG_HTTP_COMPRESSION_ENABLED 1
#else
#define SYSLOG_NG_HTTP_COMPRESSION_ENABLED 0
#endif

enum CurlCompressionTypes
{
  CURL_COMPRESSION_UNKNOWN,

  CURL_COMPRESSION_UNCOMPRESSED,
  CURL_COMPRESSION_DEFAULT = CURL_COMPRESSION_UNCOMPRESSED,
  CURL_COMPRESSION_GZIP,
  CURL_COMPRESSION_DEFLATE,
};

extern gchar *CURL_COMPRESSION_LITERAL_ALL;

typedef struct Compressor Compressor;

const gchar *compressor_get_encoding_name(Compressor *self);
gboolean compressor_compress(Compressor *self, GString *compressed, const GString *message);
void compressor_free(Compressor *self);

#if SYSLOG_NG_HTTP_COMPRESSION_ENABLED
typedef struct GzipCompressor GzipCompressor;

Compressor *gzip_compressor_new(void);

typedef struct DeflateCompressor DeflateCompressor;

Compressor *deflate_compressor_new(void);
#endif

Compressor *
construct_compressor_by_type(enum CurlCompressionTypes type);
enum CurlCompressionTypes
compressor_lookup_type(const gchar *name);

#endif //SYSLOG_NG_COMPRESSION_H
