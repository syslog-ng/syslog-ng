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
type_hint_parse(const gchar *hint, TypeHint *out_type, GError **error)
{
  if (hint == NULL)
    {
      *out_type = TYPE_HINT_STRING;
      return TRUE;
    }

  if (strcmp(hint, "string") == 0)
    *out_type = TYPE_HINT_STRING;
  else if (strcmp(hint, "literal") == 0)
    *out_type = TYPE_HINT_LITERAL;
  else if (strcmp(hint, "int32") == 0 || strcmp(hint, "int") == 0)
    *out_type = TYPE_HINT_INT32;
  else if (strcmp(hint, "int64") == 0)
    *out_type = TYPE_HINT_INT64;
  else if (strcmp(hint, "double") == 0)
    *out_type = TYPE_HINT_DOUBLE;
  else if (strcmp(hint, "datetime") == 0)
    *out_type = TYPE_HINT_DATETIME;
  else if (strcmp(hint, "list") == 0)
    *out_type = TYPE_HINT_LIST;
  else if (strcmp(hint, "boolean") == 0)
    *out_type = TYPE_HINT_BOOLEAN;
  else if (strcmp(hint, "default") == 0)
    *out_type = TYPE_HINT_DEFAULT;
  else
    {
      g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_TYPE,
                  "Unknown type specified in type hinting: %s", hint);
      return FALSE;
    }

  return TRUE;
}

gboolean
type_cast_drop_helper(gint drop_flags, const gchar *value,
                      const gchar *type_hint)
{
  if (!(drop_flags & ON_ERROR_SILENT))
    {
      msg_error("Casting error",
                evt_tag_str("value", value),
                evt_tag_str("type-hint", type_hint));
    }
  return drop_flags & ON_ERROR_DROP_MESSAGE;
}

gboolean
type_cast_to_boolean(const gchar *value, gboolean *out, GError **error)
{
  if (value[0] == 'T' || value[0] == 't' || value[0] == '1')
    *out = TRUE;
  else if (value[0] == 'F' || value[0] == 'f' || value[0] == '0')
    *out = FALSE;
  else
    {
      if (error)
        g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                    "boolean(%s)", value);
      return FALSE;
    }

  return TRUE;
}

gboolean
type_cast_to_int32(const gchar *value, gint32 *out, GError **error)
{
  gchar *endptr;

  *out = (gint32)strtol(value, &endptr, 10);

  if (endptr[0] != '\0')
    {
      if (error)
        g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                    "int32(%s)", value);
      return FALSE;
    }
  return TRUE;
}

gboolean
type_cast_to_int64(const gchar *value, gint64 *out, GError **error)
{
  gchar *endptr;

  *out = (gint64)strtoll(value, &endptr, 10);

  if (endptr[0] != '\0')
    {
      if (error)
        g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                    "int64(%s)", value);
      return FALSE;
    }
  return TRUE;
}

gboolean
type_cast_to_double(const gchar *value, gdouble *out, GError **error)
{
  gchar *endptr = NULL;
  gboolean success = TRUE;

  errno = 0;
  *out = strtod(value, &endptr);
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

gboolean
type_cast_to_datetime_int(const gchar *value, guint64 *out, GError **error)
{
  gchar *endptr;

  *out = (gint64)strtoll(value, &endptr, 10) * 1000;

  if (endptr[0] == '.')
    {
      gsize len = strlen(endptr) - 1, p;
      gchar *e, tmp[4];
      glong i;

      if (len > 3)
        len = 3;

      memcpy(tmp, endptr + 1, len);
      tmp[len] = '\0';

      i = strtoll(tmp, &e, 10);

      if (e[0] != '\0')
        {
          if (error)
            g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                        "datetime(%s)", value);
          return FALSE;
        }

      for (p = 3 - len; p > 0; p--)
        i *= 10;

      *out += i;
    }
  else if (endptr[0] != '\0')
    {
      if (error)
        g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                    "datetime(%s)", value);
      return FALSE;
    }
  return TRUE;
}

gboolean
type_cast_to_datetime_str(const gchar *value, const char *format,
                          gchar **out, GError **error)
{
  if (error)
    g_set_error(error, TYPE_HINTING_ERROR, TYPE_HINTING_INVALID_CAST,
                "datetime_str is not supported yet");
  return FALSE;
}
