/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "str-format.h"

#include <string.h>
#include <ctype.h>

static gchar digits[] = "0123456789abcdef";

/* format 64 bit ints */

static inline gint
format_uint64_base10_rev(gchar *result, gsize result_len, gint sign, guint64 value)
{
  gchar *p;
  gboolean negative = FALSE;
  gboolean first = TRUE;

  if (sign && ((gint64) value) < 0)
    {
      value = -((gint64) value);
      negative = TRUE;
    }

  p = result;
  while (first || (result_len > 0 && value > 0))
    {
      *p = digits[value % 10];
      value /= 10;
      p++;
      result_len--;
      first = FALSE;
    }
  if (negative && result_len > 0)
    {
      *p = '-';
      p++;
    }
  return p - result;
}

static inline gint
format_uint64_base16_rev(gchar *result, gsize result_len, guint64 value)
{
  gchar *p;

  p = result;
  while (result_len > 0 && value > 0)
    {
      *p = digits[value & 0x0F];
      value >>= 4;
      p++;
      result_len--;
    }
  return p - result;
}

static inline gint
format_uint64_base8_rev(gchar *result, gsize result_len, guint64 value)
{
  gchar *p;
  gboolean first = TRUE;

  p = result;
  while (first || (result_len > 0 && value > 0))
    {
      *p = digits[value & 0x07];
      value >>= 3;
      p++;
      result_len--;
      first = FALSE;
    }
  return p - result;
}

static gint
format_padded_int64(GString *result, gint field_len, gchar pad_char, gint sign, gint base, guint64 value)
{
  gchar num[64];
  gint len, i, pos;

  if (base == 10)
    len = format_uint64_base10_rev(num, sizeof(num), sign, value);
  else if (base == 16)
    len = format_uint64_base16_rev(num, sizeof(num), value);
  else if (base == 8)
    len = format_uint64_base8_rev(num, sizeof(num), value);
  else
    g_assert_not_reached();

  if (field_len == 0 || field_len < len)
    field_len = len;

  pos = result->len;
  if (G_UNLIKELY(result->allocated_len < pos + field_len + 1))
    {
      g_string_set_size(result, pos + field_len);
    }
  else
    {
      result->len += field_len;
      result->str[pos + field_len] = 0;
    }

  memset(result->str + pos, pad_char, field_len - len);
  for (i = 0; i < len; i++)
    {
      result->str[pos + field_len - i - 1] = num[i];
    }
  return field_len;
}

gint
format_uint64_padded(GString *result, gint field_len, gchar pad_char, gint base, guint64 value)
{
  return format_padded_int64(result, field_len, pad_char, 0, base, value);
}

gint
format_int64_padded(GString *result, gint field_len, gchar pad_char, gint base, gint64 value)
{
  return format_padded_int64(result, field_len, pad_char, 1, base, value);
}

/* format 32 bit ints */

static inline gint
format_uint32_base10_rev(gchar *result, gsize result_len, gint sign, guint32 value)
{
  gchar *p;
  gboolean negative = 0;
  gboolean first = TRUE;

  if (sign && ((gint32) value) < 0)
    {
      value = -((gint32) value);
      negative = 1;
    }

  p = result;
  while (first || (result_len > 0 && value > 0))
    {
      *p = digits[value % 10];
      value /= 10;
      p++;
      result_len--;
      first = FALSE;
    }
  if (negative && result_len > 0)
    {
      *p = '-';
      p++;
    }
  return p - result;
}

static inline gint
format_uint32_base16_rev(gchar *result, gsize result_len, guint32 value)
{
  gchar *p;
  gboolean first = TRUE;

  p = result;
  while (first || (result_len > 0 && value > 0))
    {
      *p = digits[value & 0x0F];
      value >>= 4;
      p++;
      result_len--;
      first = FALSE;
    }
  return p - result;
}

static inline gint
format_uint32_base8_rev(gchar *result, gsize result_len, guint32 value)
{
  gchar *p;
  gboolean first = TRUE;

  p = result;
  while (first || (result_len > 0 && value > 0))
    {
      *p = digits[value & 0x07];
      value >>= 3;
      p++;
      result_len--;
      first = FALSE;
    }
  return p - result;
}

static gint
format_padded_int32(GString *result, gint field_len, gchar pad_char, gint sign, gint base, guint32 value)
{
  gchar num[32];
  gint len, i, pos;

  if (base == 10)
    len = format_uint32_base10_rev(num, sizeof(num), sign, value);
  else if (base == 16)
    len = format_uint32_base16_rev(num, sizeof(num), value);
  else if (base == 8)
    len = format_uint32_base8_rev(num, sizeof(num), value);
  else
    g_assert_not_reached();

  if (field_len == 0 || field_len < len)
    field_len = len;

  pos = result->len;
  if (G_UNLIKELY(result->allocated_len < pos + field_len + 1))
    {
      g_string_set_size(result, pos + field_len);
    }
  else
    {
      result->len += field_len;
      result->str[pos + field_len] = 0;
    }

  memset(result->str + pos, pad_char, field_len - len);
  for (i = 0; i < len; i++)
    {
      result->str[pos + field_len - i - 1] = num[i];
    }
  return field_len;
}

gint
format_uint32_padded(GString *result, gint field_len, gchar pad_char, gint base, guint32 value)
{
  return format_padded_int32(result, field_len, pad_char, 0, base, value);
}

gint
format_int32_padded(GString *result, gint field_len, gchar pad_char, gint base, gint32 value)
{
  return format_padded_int32(result, field_len, pad_char, 1, base, value);
}

gchar *
format_hex_string_with_delimiter(gpointer data, gsize data_len, gchar *result, gsize result_len, gchar delimiter)
{
  gint i;
  gint pos = 0;
  guchar *str = (guchar *) data;

  for (i = 0; i < data_len && result_len - pos >= 3; i++)
    {
      if ( (delimiter != 0) && (i < data_len - 1))
        {
          g_snprintf(result + pos, 4, "%02x%c", str[i], delimiter);
          pos += 3;
        }
      else
        {
          g_snprintf(result + pos, 3, "%02x", str[i]);
          pos += 2;
        }
    }
  return result;
}

gchar *
format_hex_string(gpointer data, gsize data_len, gchar *result, gsize result_len)
{
  return format_hex_string_with_delimiter(data, data_len, result, result_len, 0);
}

/* parse 32 bit ints */

gboolean
scan_uint32(const gchar **buf, gint *left, gint field_width, guint32 *num)
{
  guint32 result;

  result = 0;

  while (*left > 0 && field_width > 0)
    {
      if ((**buf) >= '0' && (**buf) <= '9')
        result = result * 10 + ((**buf) - '0');
      else if (!isspace((int) **buf))
        return FALSE;
      (*buf)++;
      (*left)--;
      field_width--;
    }
  if (field_width != 0)
    return FALSE;
  *num = result;
  return TRUE;
}

gboolean
scan_int(const gchar **buf, gint *left, gint field_width, gint *num)
{
  guint32 value;

  if (!scan_uint32(buf, left, field_width, &value))
    return FALSE;
  *num = (gint) value;
  return TRUE;
}

gboolean
scan_expect_char(const gchar **buf, gint *left, gchar value)
{
  if (*left == 0)
    return FALSE;
  if ((**buf) != value)
    return FALSE;
  (*buf)++;
  (*left)--;
  return TRUE;
}

gboolean
scan_expect_str(const gchar **buf, gint *left, const gchar *value)
{
  const gchar *original_buf = *buf;
  gint original_left = *left;

  while (*value)
    {
      if (*left == 0 || *value != **buf)
        {
          *buf = original_buf;
          *left = original_left;
          return FALSE;
        }

      (*buf)++;
      (*left)--;
      value++;
    }

  return TRUE;
}
