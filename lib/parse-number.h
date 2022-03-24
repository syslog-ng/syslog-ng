/*
 * Copyright (c) 2013-2014 Balabit
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#ifndef PARSE_NUMBER_H_INCLUDED
#define PARSE_NUMBER_H_INCLUDED

#include "generic-number.h"

gboolean parse_int64(const gchar *str, gint64 *result);
gboolean parse_int64_base16(const gchar *str, gint64 *result);
gboolean parse_int64_base8(const gchar *str, gint64 *result);
gboolean parse_int64_base_any(const gchar *str, gint64 *result);

gboolean parse_int64_with_suffix(const gchar *str, gint64 *result);
gboolean parse_double(const gchar *str, gdouble *result);
gboolean parse_generic_number(const char *str, GenericNumber *number);

#endif
