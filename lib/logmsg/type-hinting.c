/*
 * Copyright (c) 2012-2014 Balabit
 * Copyright (c) 2012-2014 Gergely Nagy <algernon@balabit.hu>
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

#include "messages.h"
#include "type-hinting.h"
#include "template/templates.h"
#include "timeutils/scan-timestamp.h"
#include "str-utils.h"

#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

GQuark
type_hinting_error_quark(void)
{
  return g_quark_from_static_string("type-hinting-error-quark");
}

gboolean
type_hint_parse(const gchar *hint, LogMessageValueType *out_type, GError **error)
{
  if (!log_msg_value_type_from_str(hint, out_type) || *out_type == LM_VT_NONE)
    {
      g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_TYPE,
                  "Unknown type specified in type hinting: %s", hint);
      return FALSE;
    }

  return TRUE;
}

gboolean
type_cast_drop_helper(gint drop_flags, const gchar *value, gssize value_len,
                      const gchar *type_hint)
{
  if (!(drop_flags & ON_ERROR_SILENT))
    {
      msg_error("Casting error",
                evt_tag_mem("value", value, value_len < 0 ? strlen(value) : value_len),
                evt_tag_str("type-hint", type_hint));
    }
  return drop_flags & ON_ERROR_DROP_MESSAGE;
}

gboolean
type_cast_to_boolean(const gchar *value, gssize value_len, gboolean *out, GError **error)
{
  if (value_len == 0)
    return FALSE;

  if (value[0] == 'T' || value[0] == 't' || value[0] == '1')
    *out = TRUE;
  else if (value[0] == 'F' || value[0] == 'f' || value[0] == '0')
    *out = FALSE;
  else
    {
      if (error)
        {
          if (value_len < 0)
            value_len = strlen(value);
          g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                      "boolean(%.*s)", (gint) value_len, value);
        }
      return FALSE;
    }

  return TRUE;
}

static gboolean
_is_value_hex(const gchar *value)
{
  if (value[0] == '+' || value[0] == '-')
    value++;
  return (value[0] == '0' && (value[1] == 'x' || value[1] == 'X'));
}

gboolean
type_cast_to_int32(const gchar *value, gssize value_len, gint32 *out, GError **error)
{
  gchar *endptr;

  if (value_len >= 0)
    APPEND_ZERO(value, value, value_len);

  if (_is_value_hex(value))
    *out = (gint32)strtol(value, &endptr, 16);
  else
    *out = (gint32)strtol(value, &endptr, 10);

  if (value[0] == 0 || endptr[0] != '\0')
    {
      if (error)
        g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                    "int32(%s)", value);
      return FALSE;
    }
  return TRUE;
}

gboolean
type_cast_to_int64(const gchar *value, gssize value_len, gint64 *out, GError **error)
{
  gchar *endptr;

  if (value_len >= 0)
    APPEND_ZERO(value, value, value_len);

  if (_is_value_hex(value))
    *out = (gint64)strtoll(value, &endptr, 16);
  else
    *out = (gint64)strtoll(value, &endptr, 10);

  if (value[0] == 0 || endptr[0] != '\0')
    {
      if (error)
        g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                    "int64(%s)", value);
      return FALSE;
    }
  return TRUE;
}

gboolean
type_cast_to_double(const gchar *value, gssize value_len, gdouble *out, GError **error)
{
  gchar *endptr = NULL;
  gboolean success = TRUE;

  if (value_len >= 0)
    APPEND_ZERO(value, value, value_len);

  errno = 0;
  *out = g_ascii_strtod(value, &endptr);
  if (errno == ERANGE && (*out >= HUGE_VAL || *out <= -HUGE_VAL))
    success = FALSE;
  if (endptr == value)
    success = FALSE;
  if (endptr[0] != '\0')
    success = FALSE;

  if (!success && error)
    {
      g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                  "double(%s)", value);
    }

  return success;
}

static gboolean
_parse_fixed_point_timestamp_in_nsec(const gchar *value, gchar **endptr, gint64 *sec, gint64 *nsec)
{
  *nsec = 0;

  *sec = (gint64) strtoll(value, endptr, 10);
  if (**endptr == '.')
    {
      const gchar *nsec_start = (*endptr) + 1;

      *nsec = (gint64) strtoll(nsec_start, endptr, 10);
      gint nsec_length = (*endptr) - nsec_start;
      if (nsec_length == 0)
        return FALSE;

      if (nsec_length > 9)
        return FALSE;

      for (gint i = 0; i < 9 - nsec_length; i++)
        *nsec *= 10;
      return TRUE;
    }
  return TRUE;
}

gboolean
type_cast_to_datetime_unixtime(const gchar *value, gssize value_len, UnixTime *ut, GError **error)
{
  gchar *endptr;
  gint64 sec, nsec;
  gint tzofs = -1;

  if (value_len >= 0)
    APPEND_ZERO(value, value, value_len);

  if (!_parse_fixed_point_timestamp_in_nsec(value, &endptr, &sec, &nsec))
    goto error;

  const guchar *tz_start = (guchar *) endptr;
  if (*tz_start != 0)
    {
      gint tz_length = strlen(endptr);
      if (!scan_iso_timezone(&tz_start, &tz_length, &tzofs))
        goto error;
    }

  ut->ut_sec = sec;
  ut->ut_usec = nsec / 1000;
  ut->ut_gmtoff = tzofs;
  return TRUE;
error:

  if (error)
    g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                "datetime(%s)", value);
  return FALSE;

}

gboolean
type_cast_to_datetime_msec(const gchar *value, gssize value_len, gint64 *out, GError **error)
{
  UnixTime ut;

  if (!type_cast_to_datetime_unixtime(value, value_len, &ut, error))
    return FALSE;

  *out = ut.ut_sec * 1000 + ut.ut_usec / 1000;
  return TRUE;
}

gboolean
type_cast_validate(const gchar *value, gssize value_len, LogMessageValueType type, GError **error)
{
  gboolean b;
  gint64 i64;
  gdouble d;
  UnixTime ut;

  switch (type)
    {
    case LM_VT_STRING:
    case LM_VT_JSON:
    case LM_VT_NULL:
    case LM_VT_BYTES:
    case LM_VT_PROTOBUF:
    case LM_VT_LIST:
      return TRUE;
    case LM_VT_BOOLEAN:
      return type_cast_to_boolean(value, value_len, &b, error);
    case LM_VT_INTEGER:
      return type_cast_to_int64(value, value_len, &i64, error);
    case LM_VT_DOUBLE:
      return type_cast_to_double(value, value_len, &d, error);
    case LM_VT_DATETIME:
      return type_cast_to_datetime_unixtime(value, value_len, &ut, error);
    default:
      g_assert_not_reached();
    }
  return TRUE;
}
