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

#define _DEFLATE_WBITS_DEFLATE MAX_WBITS
#define _DEFLATE_WBITS_GZIP MAX_WBITS + 16

#include "compression.h"
#include "messages.h"
#include <zlib.h>

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
#ifdef SYSLOG_NG_HAVE_ZLIB
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_GZIP)) return TRUE;
#endif
#ifdef SYSLOG_NG_HAVE_ZLIB
  if(http_dd_curl_compression_string_match(type, CURL_COMPRESSION_DEFLATE)) return TRUE;
#endif
  return FALSE;
}

struct Compressor
{
  gboolean (*compress) (Compressor *, GString *, const GString *);
  void (*free_fn) (Compressor *self);
};

gboolean
compressor_compress(Compressor *self, GString *compressed, const GString *message)
{
  return self->compress(self, compressed, message);
}

void
compressor_free(Compressor *self)
{
  if(self != NULL)
    {
      self->free_fn(self);
      g_free(self);
    }
}

void
compressor_free_method(Compressor *self)
{

}

void
compressor_init_instance(Compressor *self)
{
  self->free_fn = compressor_free_method;
}

enum _DeflateAlgorithmTypes
{
  DEFLATE_TYPE_DEFLATE,
  DEFLATE_TYPE_GZIP
};

const gchar *_compression_error_message = "Failed due to %s error.";
static inline void
_handle_compression_error(GString *compression_dest, gchar *error_description)
{
}

static inline gboolean
_raise_compression_status(GString *compression_dest, int algorithm_exit)
{
  return FALSE;
}

static inline void
_allocate_compression_output_buffer(GString *compression_buffer, guint input_size)
{
  g_string_set_size(compression_buffer, (guint)(input_size * 1.1) + 22);
}

static inline int
_set_deflate_type_wbit(enum _DeflateAlgorithmTypes deflate_algorithm_type)
{
  switch (deflate_algorithm_type)
    {
    case DEFLATE_TYPE_DEFLATE:
      return _DEFLATE_WBITS_DEFLATE;
    case DEFLATE_TYPE_GZIP:
      return _DEFLATE_WBITS_GZIP;
    default:
      g_assert_not_reached();
    }
}

static inline int
_z_stream_init(z_stream *compress_stream, GString *compressed, const GString *message)
{
  compress_stream->data_type = Z_TEXT;

  compress_stream->next_in = (guchar *)message->str;
  compress_stream->avail_in = message->len;
  compress_stream->total_in = compress_stream->avail_in;

  _allocate_compression_output_buffer(compressed, compress_stream->avail_in);

  //Check buffer overrun
  if(compress_stream->avail_in != message->len)
    {
      return Z_BUF_ERROR;
    }
  compress_stream->next_out = (guchar *)compressed->str;
  compress_stream->avail_out = compressed->len;
  compress_stream->total_out = compress_stream->avail_out;
  if(compress_stream->avail_out != compressed->len)
    {
      return Z_BUF_ERROR;
    }
  return Z_OK;
}

static inline int
_deflate_type_compression_method( GString *destination, z_stream *compress_stream, int wbits)
{
  int err;
  err = deflateInit2(compress_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, wbits, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  if (err != Z_OK && err != Z_STREAM_END) return err;
  err = Z_OK;
  while(TRUE)
    {
      err = deflate(compress_stream, Z_FINISH);
      if (err != Z_OK && err != Z_STREAM_END)
        break;
      if (err == Z_STREAM_END)
        {
          err = Z_OK;
          deflateEnd(compress_stream);
          g_string_set_size(destination, destination->len - compress_stream->avail_out);
          break;
        }
    }
  return err;
}

int
_deflate_type_compression(GString *compressed, const GString *message,
                          const gint deflate_algorithm_type)
{
  z_stream _compress_stream = {0};
  gint err;

  err = _z_stream_init(&_compress_stream, compressed, message);
  if (err != Z_OK)
    return err;

  gint _wbits = _set_deflate_type_wbit(deflate_algorithm_type);

  return _deflate_type_compression_method(compressed, &_compress_stream, _wbits);
}

struct GzipCompressor
{
  Compressor super;
};

gboolean
_gzip_compressor_compress(Compressor *self, GString *compressed, const GString *message)
{
  int err = _deflate_type_compression(compressed, message, DEFLATE_TYPE_GZIP);
  return _raise_compression_status(compressed, err);
}

Compressor *
gzip_compressor_new(void)
{
  GzipCompressor *rval = g_new0(struct GzipCompressor, 1);
  compressor_init_instance(&rval->super);
  rval->super.compress = _gzip_compressor_compress;
  return &rval->super;
}

struct DeflateCompressor
{
  Compressor super;
};

gboolean
_deflate_compressor_compress(Compressor *self, GString *compressed, const GString *message)
{
  int err = _deflate_type_compression(compressed, message, DEFLATE_TYPE_DEFLATE);
  return _raise_compression_status(compressed, err);
}

Compressor *
deflate_compressor_new(void)
{
  DeflateCompressor *rval = g_new0(struct DeflateCompressor, 1);
  compressor_init_instance(&rval->super);
  rval->super.compress = _deflate_compressor_compress;
  return &rval->super;
}
