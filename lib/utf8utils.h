/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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
#ifndef UTF8UTILS_H_INCLUDED
#define UTF8UTILS_H_INCLUDED

#include "syslog-ng.h"

void append_unsafe_utf8_as_escaped_binary(GString *escaped_string, const gchar *str,
                                          gssize str_len, const gchar *unsafe_chars);
gchar *convert_unsafe_utf8_to_escaped_binary(const gchar *str, gssize str_len,
                                             const gchar *unsafe_chars);

void append_unsafe_utf8_as_escaped_text(GString *escaped_string, const gchar *str,
                                        gssize str_len, const gchar *unsafe_chars);
gchar *convert_unsafe_utf8_to_escaped_text(const gchar *str, gssize str_len,
                                           const gchar *unsafe_chars);


void append_unsafe_utf8_as_escaped(GString *escaped_output, const gchar *raw,
                                   gssize raw_len, const gchar *unsafe_chars,
                                   const gchar *control_format,
                                   const gchar *invalid_format);


/* for performance-critical use only */

#define SANITIZE_UTF8_BUFFER_SIZE(l) (l * 6 + 1)

static inline const gchar *
optimized_sanitize_utf8_to_escaped_binary(const guchar *data, gint length, gsize *new_length, gchar *out,
                                          gsize out_size)
{
  GString sanitized_message;

  /* avoid GString allocation */
  sanitized_message.str = out;
  sanitized_message.len = 0;
  sanitized_message.allocated_len = out_size;

  append_unsafe_utf8_as_escaped_binary(&sanitized_message, (const gchar *) data, length, NULL);

  /* MUST NEVER BE REALLOCATED */
  g_assert(sanitized_message.str == out);

  *new_length = sanitized_message.len;
  return out;
}

#endif
