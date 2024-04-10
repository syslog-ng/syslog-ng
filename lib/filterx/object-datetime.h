/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#ifndef FILTERX_OBJECT_DATETIME_H_INCLUDED
#define FILTERX_OBJECT_DATETIME_H_INCLUDED

#include "filterx-object.h"

FILTERX_DECLARE_TYPE(datetime);

#define datefmt_isodate "%Y-%m-%dT%H:%M:%S%z"

FilterXObject *filterx_datetime_new(const UnixTime *ut);
UnixTime filterx_datetime_get_value(FilterXObject *s);
FilterXObject *filterx_typecast_datetime(GPtrArray *args);
FilterXObject *filterx_typecast_datetime_isodate(GPtrArray *args);
FilterXObject *filterx_datetime_strptime(GPtrArray *args);

#endif
