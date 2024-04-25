/*
 * Copyright (c) 2024 Attila Szakacs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "filterx-cached-json-file.h"
#include "filterx/object-json.h"

typedef struct FilterXFunctionCachedJsonFile_
{
  FilterXFunction super;
  FilterXObject *cached_json;
} FilterXFuntionCachedJsonFile;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFuntionCachedJsonFile *self = (FilterXFuntionCachedJsonFile *) s;

  return filterx_object_ref(self->cached_json);
}

static void
_free(FilterXExpr *s)
{
  FilterXFuntionCachedJsonFile *self = (FilterXFuntionCachedJsonFile *) s;

  filterx_object_unref(self->cached_json);
  filterx_function_free_method(&self->super);
}

FilterXFunction *
filterx_function_cached_json_file_new(const gchar *function_name, GList *argument_expressions, GError **error)
{
  FilterXFuntionCachedJsonFile *self = g_new0(FilterXFuntionCachedJsonFile, 1);
  filterx_function_init_instance(&self->super, function_name);

  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  self->cached_json = filterx_json_object_new_empty();

  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  return &self->super;
}

gpointer
filterx_function_cached_json_file_new_construct(Plugin *self)
{
  return (gpointer) &filterx_function_cached_json_file_new;
}
