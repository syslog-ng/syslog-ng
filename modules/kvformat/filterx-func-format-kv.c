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

#include "filterx-func-format-kv.h"

typedef struct FilterXFunctionFormatKV_
{
  FilterXFunction super;
} FilterXFunctionFormatKV;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFunctionFormatKV *self = (FilterXFunctionFormatKV *) s;

  return NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionFormatKV *self = (FilterXFunctionFormatKV *) s;

  filterx_function_free_method(&self->super);
}

FilterXFunction *
filterx_function_format_kv_new(const gchar *function_name, GList *argument_expressions, GError **error)
{
  FilterXFunctionFormatKV *self = g_new0(FilterXFunctionFormatKV, 1);
  filterx_function_init_instance(&self->super, function_name);

  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  return &self->super;
}

gpointer
filterx_function_construct_format_kv(Plugin *self)
{
  return (gpointer) filterx_function_format_kv_new;
}
