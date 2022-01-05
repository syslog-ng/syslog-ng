/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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
 */

#ifndef SYSLOG_NG_GENERIC_NUMBER_H_INCLUDED
#define SYSLOG_NG_GENERIC_NUMBER_H_INCLUDED

#include "syslog-ng.h"

typedef struct _GenericNumber
{
  enum
  {
    GN_SIGNED_INTEGER,
    GN_UNSIGNED_INTEGER,
    GN_DOUBLE,
  } type;
  union
  {
    gint64 raw_int64;
    guint64 raw_uint64;
    gdouble raw_double;
  } value;
  gint precision:4;
} GenericNumber;

void gn_set_double(GenericNumber *number, double value, gint precision);
gdouble gn_as_double(const GenericNumber *number);
void gn_set_int64(GenericNumber *number, gint64 value);
gint64 gn_as_int64(const GenericNumber *number);
void gn_set_uint64(GenericNumber *number, guint64 value);
guint64 gn_as_uint64(const GenericNumber *number);
gboolean gn_is_zero(const GenericNumber *number);
gint gn_compare(const GenericNumber *left, const GenericNumber *right);

#endif
