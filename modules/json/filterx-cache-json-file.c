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

#include "filterx-cache-json-file.h"
#include "filterx/object-json.h"
#include "filterx/object-string.h"
#include "filterx/expr-literal.h"
#include "filterx/filterx-eval.h"
#include "scratch-buffers.h"

#include "compat/json.h"

#include <stdio.h>
#include <errno.h>

#define FILTERX_FUNC_CACHE_JSON_FILE_USAGE "Usage: cache_json_file(\"/path/to/file.json\")"

#define CACHE_JSON_FILE_ERROR cache_json_file_error_quark()

enum FilterXFunctionCacheJsonFileError
{
  CACHE_JSON_FILE_ERROR_FILE_OPEN_ERROR,
  CACHE_JSON_FILE_ERROR_FILE_READ_ERROR,
  CACHE_JSON_FILE_ERROR_JSON_PARSE_ERROR,
};

static GQuark
cache_json_file_error_quark(void)
{
  return g_quark_from_static_string("filterx-function-cache-json-file-error-quark");
}

typedef struct FilterXFunctionCacheJsonFile_
{
  FilterXFunction super;
  gchar *filepath;
  FilterXObject *cached_json;
} FilterXFuntionCacheJsonFile;

static gchar *
_extract_filepath(FilterXFunctionArgs *args, GError **error)
{
  if (filterx_function_args_len(args) != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_CACHE_JSON_FILE_USAGE);
      return NULL;
    }

  gsize filepath_len;
  const gchar *filepath = filterx_function_args_get_literal_string(args, 0, &filepath_len);
  if (!filepath)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be string literal. " FILTERX_FUNC_CACHE_JSON_FILE_USAGE);
      return NULL;
    }

  return g_strdup(filepath);
}

static FilterXObject *
_load_json_file(const gchar *filepath, GError **error)
{
  FILE *file = fopen(filepath, "rb");
  if (!file)
    {
      g_set_error(error, CACHE_JSON_FILE_ERROR, CACHE_JSON_FILE_ERROR_FILE_OPEN_ERROR,
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
            g_set_error(error, CACHE_JSON_FILE_ERROR, CACHE_JSON_FILE_ERROR_FILE_READ_ERROR,
                        "failed to read file: %s (%s)", filepath, g_strerror(errno));
          break;
        }

      object = json_tokener_parse_ex(tokener, buffer, bytes_read);

      enum json_tokener_error parse_result = json_tokener_get_error(tokener);
      if (parse_result == json_tokener_success)
        break;
      if (parse_result == json_tokener_continue)
        continue;

      g_set_error(error, CACHE_JSON_FILE_ERROR, CACHE_JSON_FILE_ERROR_JSON_PARSE_ERROR,
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
  FilterXFuntionCacheJsonFile *self = (FilterXFuntionCacheJsonFile *) s;

  return filterx_object_ref(self->cached_json);
}

static void
_free(FilterXExpr *s)
{
  FilterXFuntionCacheJsonFile *self = (FilterXFuntionCacheJsonFile *) s;

  g_free(self->filepath);
  if (self->cached_json)
    filterx_object_unfreeze_and_free(self->cached_json);
  filterx_function_free_method(&self->super);
}

FilterXFunction *
filterx_function_cache_json_file_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFuntionCacheJsonFile *self = g_new0(FilterXFuntionCacheJsonFile, 1);
  filterx_function_init_instance(&self->super, function_name);

  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  self->filepath = _extract_filepath(args, error);
  if (!self->filepath)
    goto error;

  self->cached_json = _load_json_file(self->filepath, error);
  if (!self->cached_json)
    goto error;

  filterx_object_freeze(self->cached_json);
  filterx_function_args_free(args);
  return &self->super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

gpointer
filterx_function_cache_json_file_new_construct(Plugin *self)
{
  return (gpointer) &filterx_function_cache_json_file_new;
}
