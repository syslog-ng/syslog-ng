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
#include "filterx/object-string.h"
#include "filterx/expr-literal.h"
#include "filterx/filterx-eval.h"
#include "scratch-buffers.h"

#include "compat/json.h"

#include <stdio.h>
#include <errno.h>

#define FILTERX_FUNC_CACHED_JSON_FILE_USAGE "Usage: cached_json_file(\"/path/to/file.json\")"

#define CACHED_JSON_FILE_ERROR cached_json_file_error_quark()

enum FilterXFunctionCachedJsonFileError
{
  CACHED_JSON_FILE_ERROR_FILE_OPEN_ERROR,
  CACHED_JSON_FILE_ERROR_FILE_READ_ERROR,
  CACHED_JSON_FILE_ERROR_JSON_PARSE_ERROR,
};

static GQuark
cached_json_file_error_quark(void)
{
  return g_quark_from_static_string("filterx-function-cached-json-file-error-quark");
}

typedef struct FilterXFunctionCachedJsonFile_
{
  FilterXFunction super;
  gchar *filepath;
  FilterXObject *cached_json;
} FilterXFuntionCachedJsonFile;

static gchar *
_extract_filepath(GList *argument_expressions, GError **error)
{
  if (argument_expressions == NULL || g_list_length(argument_expressions) != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_CACHED_JSON_FILE_USAGE);
      return NULL;
    }

  FilterXExpr *filepath_expr = (FilterXExpr *) argument_expressions->data;
  if (!filepath_expr || !filterx_expr_is_literal(filepath_expr))
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be string literal. " FILTERX_FUNC_CACHED_JSON_FILE_USAGE);
      return NULL;
    }

  FilterXObject *filepath_obj = filterx_expr_eval(filepath_expr);
  if (!filepath_obj)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "failed to evaluate argument. " FILTERX_FUNC_CACHED_JSON_FILE_USAGE);
      return NULL;
    }

  gsize filepath_len;
  gchar *filepath = g_strdup(filterx_string_get_value(filepath_obj, &filepath_len));
  filterx_object_unref(filepath_obj);

  if (!filepath)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be string literal. " FILTERX_FUNC_CACHED_JSON_FILE_USAGE);
      g_free(filepath);
      return NULL;
    }

  return filepath;
}

static FilterXObject *
_load_json_file(const gchar *filepath, GError **error)
{
  FILE *file = fopen(filepath, "rb");
  if (!file)
    {
      g_set_error(error, CACHED_JSON_FILE_ERROR, CACHED_JSON_FILE_ERROR_FILE_OPEN_ERROR,
                  "failed to open file: %s (%s)", filepath, g_strerror(errno));
      return FALSE;
    }

  struct json_tokener *tokener = json_tokener_new();
  struct json_object *object = NULL;

  gchar buffer[1024];
  while (TRUE)
    {
      gsize bytes_read = fread(buffer, 1, sizeof(buffer), file);
      if (bytes_read <= 0)
        {
          if (ferror(file))
            g_set_error(error, CACHED_JSON_FILE_ERROR, CACHED_JSON_FILE_ERROR_FILE_READ_ERROR,
                        "failed to read file: %s (%s)", filepath, g_strerror(errno));
          break;
        }

      object = json_tokener_parse_ex(tokener, buffer, bytes_read);

      enum json_tokener_error parse_result = json_tokener_get_error(tokener);
      if (parse_result == json_tokener_success)
        break;
      if (parse_result == json_tokener_continue)
        continue;

      g_set_error(error, CACHED_JSON_FILE_ERROR, CACHED_JSON_FILE_ERROR_JSON_PARSE_ERROR,
                  "failed to parse JSON file: %s (%s)", filepath, json_tokener_error_desc(parse_result));
      break;
    }

  FilterXObject *result = NULL;
  if (object)
    {
      result = filterx_json_new_from_object(object);
      filterx_object_make_readonly(result);
    }

  json_tokener_free(tokener);
  fclose(file);
  return result;
}

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

  g_free(self->filepath);
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

  self->filepath = _extract_filepath(argument_expressions, error);
  if (!self->filepath)
    goto error;

  self->cached_json = _load_json_file(self->filepath, error);
  if (!self->cached_json)
    goto error;

  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  return &self->super;

error:
  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

gpointer
filterx_function_cached_json_file_new_construct(Plugin *self)
{
  return (gpointer) &filterx_function_cached_json_file_new;
}
