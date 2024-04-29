/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx/func-len.h"
#include "filterx/object-primitive.h"

#define FILTERX_FUNC_LEN_USAGE "Usage: len(object)"

FilterXObject *
filterx_simple_function_len(GPtrArray *args)
{
  if (args == NULL || args->len != 1)
    {
      msg_error("FilterX: len: invalid number of arguments. " FILTERX_FUNC_LEN_USAGE);
      return NULL;
    }

  FilterXObject *object = g_ptr_array_index(args, 0);
  if (!object)
    {
      msg_error("FilterX: len: invalid argument: object." FILTERX_FUNC_LEN_USAGE);
      return NULL;
    }

  guint64 len;
  gboolean success = filterx_object_len(object, &len);
  if (!success)
    {
      msg_error("FilterX: len: object type is not supported",
                evt_tag_str("type", object->type->name));
      return NULL;
    }

  return filterx_integer_new((gint64) MIN(len, G_MAXINT64));
}