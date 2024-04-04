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
#include "filterx/filterx-globals.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-datetime.h"
#include "filterx/object-message-value.h"
#include "filterx/object-list-interface.h"
#include "filterx/object-dict-interface.h"

static GHashTable *filterx_builtin_functions = NULL;

gboolean
filterx_builtin_function_register_inner(GHashTable *ht, const gchar *fn_name, FilterXFunctionProto func)
{
  return g_hash_table_insert(ht, g_strdup(fn_name), func);
}

gboolean
filterx_builtin_function_register(const gchar *fn_name, FilterXFunctionProto func)
{
  return filterx_builtin_function_register_inner(filterx_builtin_functions, fn_name, func);
}

FilterXFunctionProto
filterx_builtin_function_lookup_inner(GHashTable *ht, const gchar *fn_name)
{
  return (FilterXFunctionProto)g_hash_table_lookup(ht, fn_name);
}

FilterXFunctionProto
filterx_builtin_function_lookup(const gchar *fn_name)
{
  return filterx_builtin_function_lookup_inner(filterx_builtin_functions, fn_name);
}

void
filterx_builtin_functions_init_inner(GHashTable **ht)
{
  *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) NULL);
}

void
filterx_builtin_functions_init(void)
{
  filterx_builtin_functions_init_inner(&filterx_builtin_functions);
  filterx_builtin_function_register("json", filterx_json_new_from_args);
  filterx_builtin_function_register("json_array", filterx_json_array_new_from_args);
  g_assert(filterx_builtin_function_register("datetime", filterx_typecast_datetime));
  g_assert(filterx_builtin_function_register("isodate", filterx_typecast_datetime_isodate));
  g_assert(filterx_builtin_function_register("string", filterx_typecast_string));
  g_assert(filterx_builtin_function_register("bytes", filterx_typecast_bytes));
  g_assert(filterx_builtin_function_register("protobuf", filterx_typecast_protobuf));
  g_assert(filterx_builtin_function_register("bool", filterx_typecast_boolean));
}

void
filterx_builtin_functions_deinit_inner(GHashTable *ht)
{
  g_hash_table_destroy(ht);
}

void
filterx_builtin_functions_deinit(void)
{
  filterx_builtin_functions_deinit_inner(filterx_builtin_functions);
}

void
filterx_global_init(void)
{
  filterx_type_init(&FILTERX_TYPE_NAME(list));
  filterx_type_init(&FILTERX_TYPE_NAME(dict));

  filterx_type_init(&FILTERX_TYPE_NAME(null));
  filterx_type_init(&FILTERX_TYPE_NAME(integer));
  filterx_type_init(&FILTERX_TYPE_NAME(boolean));
  filterx_type_init(&FILTERX_TYPE_NAME(double));

  filterx_type_init(&FILTERX_TYPE_NAME(string));
  filterx_type_init(&FILTERX_TYPE_NAME(bytes));
  filterx_type_init(&FILTERX_TYPE_NAME(protobuf));

  filterx_type_init(&FILTERX_TYPE_NAME(json_object));
  filterx_type_init(&FILTERX_TYPE_NAME(json_array));
  filterx_type_init(&FILTERX_TYPE_NAME(datetime));
  filterx_type_init(&FILTERX_TYPE_NAME(message_value));

  filterx_builtin_functions_init();
}

void
filterx_global_deinit(void)
{
  filterx_builtin_functions_deinit();
}

FilterXObject *filterx_typecast_get_arg(GPtrArray *args, gchar *alt_msg)
{
  if (args == NULL || args->len != 1)
    {
      msg_error(alt_msg ? alt_msg : "filterx: typecast functions must have exactly 1 argument");
      return NULL;
    }

  FilterXObject *object = g_ptr_array_index(args, 0);
  if (!object)
    {
      msg_error(alt_msg ? alt_msg : "filterx: invalid typecast argument, object can not be null" );
      return NULL;
    }

  return object;
}
