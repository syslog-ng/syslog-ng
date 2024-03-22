/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#ifndef LOGPROTO_H_INCLUDED
#define LOGPROTO_H_INCLUDED

#include "transport/logtransport.h"

#define RFC6587_MAX_FRAME_LEN_DIGITS 10

typedef enum
{
  LPS_SUCCESS,
  LPS_ERROR,
  LPS_EOF,
  LPS_PARTIAL,
  LPS_AGAIN,
} LogProtoStatus;

/*
 * log_proto_get_char_size_for_fixed_encoding:
 *
 * This function returns the number of bytes of a single character in the
 * encoding specified by the @encoding parameter, provided it is listed in
 * the limited set hard-wired in the fixed_encodings array above.
 *
 * syslog-ng sometimes needs to calculate the size of the original, raw data
 * that relates to its already utf8 converted input buffer.  For that the
 * slow solution is to actually perform the utf8 -> raw conversion, however
 * since we don't really need the actual conversion, just the size of the
 * data in bytes we can be faster than that by multiplying the number of
 * input characters with the size of the character in the known
 * fixed-length-encodings in the list above.
 *
 * This function returns 0 if the encoding is not known, in which case the
 * slow path is to be executed.
 */
static inline gint
log_proto_get_char_size_for_fixed_encoding(const gchar *encoding)
{
  static struct
  {
    const gchar *prefix;
    gint scale;
  } fixed_encodings[] =
  {
    { "ascii", 1 },
    { "us-ascii", 1 },
    { "iso-8859", 1 },
    { "iso8859", 1 },
    { "latin", 1 },
    { "ucs2", 2 },
    { "ucs-2", 2 },
    { "ucs4", 4 },
    { "ucs-4", 4 },
    { "koi", 1 },
    { "unicode", 2 },
    { "windows", 1 },
    { "wchar_t", sizeof(wchar_t) },
    { NULL, 0 }
  };
  gint scale = 0;
  gint i;

  for (i = 0; fixed_encodings[i].prefix; i++)
    {
      if (strncasecmp(encoding, fixed_encodings[i].prefix, strlen(fixed_encodings[i].prefix)) == 0)
        {
          scale = fixed_encodings[i].scale;
          break;
        }
    }
  return scale;
}

#endif
