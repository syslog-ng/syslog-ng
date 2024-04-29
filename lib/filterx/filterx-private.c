/*
 * Copyright (c) 2024 shifter <shifter@axoflow.com>
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
#include "filterx/filterx-private.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-datetime.h"
#include "filterx/object-message-value.h"
#include "filterx/object-list-interface.h"
#include "filterx/object-dict-interface.h"

// Builtin functions

gboolean
filterx_builtin_simple_function_register_private(GHashTable *ht, const gchar *fn_name, FilterXSimpleFunctionProto func)
{
  return g_hash_table_insert(ht, g_strdup(fn_name), func);
}

FilterXSimpleFunctionProto
filterx_builtin_simple_function_lookup_private(GHashTable *ht, const gchar *fn_name)
{
  return (FilterXSimpleFunctionProto)g_hash_table_lookup(ht, fn_name);
}

void
filterx_builtin_simple_functions_init_private(GHashTable **ht)
{
  *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) NULL);
}

void
filterx_builtin_simple_functions_deinit_private(GHashTable *ht)
{
  g_hash_table_destroy(ht);
}

gboolean
filterx_builtin_function_ctor_register_private(GHashTable *ht, const gchar *fn_name, FilterXFunctionCtor ctor)
{
  return g_hash_table_insert(ht, g_strdup(fn_name), ctor);
}

FilterXFunctionCtor
filterx_builtin_function_ctor_lookup_private(GHashTable *ht, const gchar *fn_name)
{
  return (FilterXFunctionCtor)g_hash_table_lookup(ht, fn_name);
}

void
filterx_builtin_function_ctors_init_private(GHashTable **ht)
{
  *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) NULL);
}

void
filterx_builtin_function_ctors_deinit_private(GHashTable *ht)
{
  g_hash_table_destroy(ht);
}

// FilterX Types

gboolean
filterx_type_register_private(GHashTable *ht, const gchar *type_name, FilterXType *fxtype)
{
  return g_hash_table_insert(ht, g_strdup(type_name), fxtype);
}

FilterXType *
filterx_type_lookup_private(GHashTable *ht, const gchar *type_name)
{
  return (FilterXType *)g_hash_table_lookup(ht, type_name);
}

void
filterx_types_init_private(GHashTable **ht)
{
  *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) NULL);
}

void
filterx_types_deinit_private(GHashTable *ht)
{
  g_hash_table_destroy(ht);
}
