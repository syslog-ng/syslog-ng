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
#include "messages.h"
#include <zlib.h>

#define _DEFLATE_WBITS_DEFLATE MAX_WBITS
#define _DEFLATE_WBITS_GZIP MAX_WBITS + 16

gchar *CURL_COMPRESSION_LITERAL_ALL = "all";
static gchar *curl_compression_types[] = {"unknown", "identity", "gzip", "deflate"};

struct Compressor
{
  const gchar *encoding_name;
  gboolean (*compress) (Compressor *, GString *, const GString *);
  void (*free_fn) (Compressor *self);
};

const gchar *
compressor_get_encoding_name(Compressor *self)
{
  return self->encoding_name;
}

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
compressor_init_instance(Compressor *self, enum CurlCompressionTypes type)
{
  self->free_fn = compressor_free_method;
  self->encoding_name = curl_compression_types[type];
}

#if SYSLOG_NG_HTTP_COMPRESSION_ENABLED
enum _DeflateAlgorithmTypes
{
  DEFLATE_TYPE_DEFLATE,
  DEFLATE_TYPE_GZIP
};

const gchar *_compression_error_message = "Failed due to %s error.";
static inline void
_handle_compression_error(GString *compression_dest, gchar *error_description)
{
  msg_error("compression", evt_tag_printf("error", _compression_error_message, error_description));
  g_string_truncate(compression_dest, 0);
}

typedef enum
{
  _COMPRESSION_OK,
  _COMPRESSION_ERR_BUFFER,
  _COMPRESSION_ERR_DATA,
  _COMPRESSION_ERR_STREAM,
  _COMPRESSION_ERR_MEMORY,
  _COMPRESSION_ERR_UNSPECIFIED
} _CompressionUnifiedErrorCode;

static inline gboolean
_raise_compression_status(GString *compression_dest, int algorithm_exit)
{
  switch (algorithm_exit)
    {
    case _COMPRESSION_OK:
      return TRUE;
    case _COMPRESSION_ERR_BUFFER:
      _handle_compression_error(compression_dest, "buffer");
      return FALSE;
    case _COMPRESSION_ERR_MEMORY:
      _handle_compression_error(compression_dest, "memory");
      return FALSE;
    case _COMPRESSION_ERR_STREAM:
      _handle_compression_error(compression_dest, "stream");
      return FALSE;
    case _COMPRESSION_ERR_DATA:
      _handle_compression_error(compression_dest, "data");
      return FALSE;
    case _COMPRESSION_ERR_UNSPECIFIED:
      _handle_compression_error(compression_dest, "unspecified");
      return FALSE;
    default:
      g_assert_not_reached();
    }
}

static inline void
_allocate_compression_output_buffer(GString *compression_buffer, guint input_size)
{
  g_string_set_size(compression_buffer, (guint)(input_size * 1.1) + 22);
}

_CompressionUnifiedErrorCode
_error_code_swap_zlib(int z_err)
{
  switch (z_err)
    {
    case Z_OK:
      return _COMPRESSION_OK;
    case Z_BUF_ERROR:
      return _COMPRESSION_ERR_BUFFER;
    case Z_MEM_ERROR:
      return _COMPRESSION_ERR_MEMORY;
    case Z_STREAM_ERROR:
      return _COMPRESSION_ERR_STREAM;
    case Z_DATA_ERROR:
      return _COMPRESSION_ERR_DATA;
    default:
      return _COMPRESSION_ERR_UNSPECIFIED;
    }
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

static inline _CompressionUnifiedErrorCode
_z_stream_init(z_stream *compress_stream, GString *compressed,
               const GString *message)
{
  compress_stream->data_type = Z_TEXT;

  compress_stream->next_in = (guchar *)message->str;
  compress_stream->avail_in = message->len;
  compress_stream->total_in = compress_stream->avail_in;

  _allocate_compression_output_buffer(compressed, compress_stream->avail_in);

  //Check buffer overrun
  if(compress_stream->avail_in != message->len)
    {
      return _COMPRESSION_ERR_BUFFER;
    }
  compress_stream->next_out = (guchar *)compressed->str;
  compress_stream->avail_out = compressed->len;
  compress_stream->total_out = compress_stream->avail_out;
  if(compress_stream->avail_out != compressed->len)
    {
      return _COMPRESSION_ERR_BUFFER;
    }
  return _COMPRESSION_OK;
}

static inline _CompressionUnifiedErrorCode
_deflate_type_compression_method( GString *destination,
                                  z_stream *compress_stream, int wbits)
{
  int err;
  err = deflateInit2(compress_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, wbits, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  if (err != Z_OK && err != Z_STREAM_END) return _error_code_swap_zlib(err);
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
  return _error_code_swap_zlib(err);
}

_CompressionUnifiedErrorCode
_deflate_type_compression(GString *compressed, const GString *message,
                          const gint deflate_algorithm_type)
{
  z_stream _compress_stream = {0};
  gint err;

  err = _z_stream_init(&_compress_stream, compressed, message);
  if (err != Z_OK)
    return _error_code_swap_zlib(err);

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
  _CompressionUnifiedErrorCode err = _deflate_type_compression(compressed, message, DEFLATE_TYPE_GZIP);
  return _raise_compression_status(compressed, err);
}

Compressor *
gzip_compressor_new(void)
{
  GzipCompressor *rval = g_new0(struct GzipCompressor, 1);
  compressor_init_instance(&rval->super, CURL_COMPRESSION_GZIP);
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
  _CompressionUnifiedErrorCode err = _deflate_type_compression(compressed, message, DEFLATE_TYPE_DEFLATE);
  return _raise_compression_status(compressed, err);
}

Compressor *
deflate_compressor_new(void)
{
  DeflateCompressor *rval = g_new0(struct DeflateCompressor, 1);
  compressor_init_instance(&rval->super, CURL_COMPRESSION_DEFLATE);
  rval->super.compress = _deflate_compressor_compress;
  return &rval->super;
}
#endif


Compressor *
construct_compressor_by_type(enum CurlCompressionTypes type)
{
  switch (type)
    {
#if SYSLOG_NG_HTTP_COMPRESSION_ENABLED
    case CURL_COMPRESSION_GZIP:
      return gzip_compressor_new();
    case CURL_COMPRESSION_DEFLATE:
      return deflate_compressor_new();
#endif
    case CURL_COMPRESSION_UNCOMPRESSED:
    default:
      return NULL;
    }
}

static gboolean
_curl_compression_string_match(const gchar *string, gint curl_compression_index)
{
  return (g_strcmp0(string, curl_compression_types[curl_compression_index]) == 0);
}

enum CurlCompressionTypes
compressor_lookup_type(const gchar *name)
{
  if (_curl_compression_string_match(name, CURL_COMPRESSION_UNCOMPRESSED))
    return CURL_COMPRESSION_UNCOMPRESSED;
#if SYSLOG_NG_HTTP_COMPRESSION_ENABLED
  if (_curl_compression_string_match(name, CURL_COMPRESSION_GZIP))
    return CURL_COMPRESSION_GZIP;
  if (_curl_compression_string_match(name, CURL_COMPRESSION_DEFLATE))
    return CURL_COMPRESSION_DEFLATE;
#endif
  return CURL_COMPRESSION_UNKNOWN;
}
