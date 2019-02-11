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

#ifndef TYPE_HINTING_H
#define TYPE_HINTING_H

#include "syslog-ng.h"

#define TYPE_HINTING_ERROR type_hinting_error_quark()

GQuark type_hinting_error_quark(void);

enum TypeHintingError
{
  TYPE_HINTING_INVALID_TYPE,
  TYPE_HINTING_INVALID_CAST,
};

typedef enum
{
  TYPE_HINT_STRING,
  TYPE_HINT_LITERAL,
  TYPE_HINT_BOOLEAN,
  TYPE_HINT_INT32,
  TYPE_HINT_INT64,
  TYPE_HINT_DOUBLE,
  TYPE_HINT_DATETIME,
  TYPE_HINT_LIST,
  TYPE_HINT_DEFAULT,
} TypeHint;

gboolean type_hint_parse(const gchar *hint, TypeHint *out_hint, GError **error);

gboolean type_cast_drop_helper(gint drop_flags, const gchar *value,
                               const gchar *type_hint);

gboolean type_cast_to_boolean(const gchar *value, gboolean *out, GError **error);
gboolean type_cast_to_int32(const gchar *value, gint32 *out, GError **error);
gboolean type_cast_to_int64(const gchar *value, gint64 *out, GError **error);
gboolean type_cast_to_double(const gchar *value, gdouble *out, GError **error);
gboolean type_cast_to_datetime_int(const gchar *value, guint64 *out, GError **error);
gboolean type_cast_to_datetime_str(const gchar *value, const char *format,
                                   gchar **out, GError **error);

#endif
