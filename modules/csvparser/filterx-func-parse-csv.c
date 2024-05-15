/*
 * Copyright (c) 2024 shifter
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

#include "filterx-func-parse-csv.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/expr-literal.h"
#include "filterx/filterx-eval.h"
#include "filterx/filterx-globals.h"
#include "filterx/object-json.h"
#include "filterx/object-message-value.h"
#include "filterx/object-null.h"
#include "filterx/filterx-object.h"
#include "filterx/object-dict-interface.h"
#include "filterx/object-list-interface.h"

#include "scanner/csv-scanner/csv-scanner.h"
#include "parser/parser-expr.h"
#include "scratch-buffers.h"
#include "str-utils.h"
#include "csvparser.h"

typedef struct FilterXFunctionParseCSV_
{
  FilterXFunction super;
  FilterXExpr *msg;
  CSVScannerOptions options;
  FilterXExpr *columns;
} FilterXFunctionParseCSV;

static gboolean
_parse_columns(FilterXFunctionParseCSV *self, GList **col_names)
{
  gboolean result = FALSE;
  if (!self->columns)
    return TRUE;
  FilterXObject *cols_obj = filterx_expr_eval(self->columns);
  if (!cols_obj)
    return FALSE;

  if (filterx_object_is_type(cols_obj, &FILTERX_TYPE_NAME(null)))
    {
      result = TRUE;
      goto exit;
    }

  if (!filterx_object_is_type(cols_obj, &FILTERX_TYPE_NAME(json_array)))
    goto exit;

  guint64 size;
  if (!filterx_object_len(cols_obj, &size))
    return FALSE;

  for (guint64 i = 0; i < size; i++)
    {
      FilterXObject *col = filterx_list_get_subscript(cols_obj, i);
      if (filterx_object_is_type(col, &FILTERX_TYPE_NAME(string)))
        {
          const gchar *col_name = filterx_string_get_value(col, NULL);
          *col_names = g_list_append(*col_names, g_strdup(col_name));
        }
      filterx_object_unref(col);
    }

  result = TRUE;
exit:
  filterx_object_unref(cols_obj);
  return result;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFunctionParseCSV *self = (FilterXFunctionParseCSV *) s;

  FilterXObject *obj = filterx_expr_eval(self->msg);
  if (!obj)
    return NULL;

  gboolean ok = FALSE;

  gsize len;
  const gchar *input;
  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)))
    input = filterx_string_get_value(obj, &len);
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(message_value))
           && filterx_message_value_get_type(obj) == LM_VT_STRING)
    input = filterx_message_value_get_value(obj, &len);
  else
    goto exit;

  APPEND_ZERO(input, input, len);

  if (!_parse_columns(self, &cols))
    goto exit;

  if (cols)
    {
      csv_scanner_options_set_expected_columns(&self->options, g_list_length(cols));
      result = filterx_json_object_new_empty();
    }
  else
    result = filterx_json_array_new_empty();

  CSVScanner scanner;
  csv_scanner_init(&scanner, &self->options, input);

  GList *col = cols;
  while (csv_scanner_scan_next(&scanner))
    {
      if (cols)
        {
          if (!col)
            break;
          FilterXObject *key = filterx_string_new(col->data, -1);
          FilterXObject *val = filterx_string_new(csv_scanner_get_current_value(&scanner),
                                                  csv_scanner_get_current_value_len(&scanner));

          ok = filterx_object_set_subscript(result, key, &val);

          filterx_object_unref(key);
          filterx_object_unref(val);

          if (!ok)
            goto exit;
          col = g_list_next(col);
        }
      else
        {
          const gchar *current_value = csv_scanner_get_current_value(&scanner);
          gint current_value_len = csv_scanner_get_current_value_len(&scanner);
          FilterXObject *val = filterx_string_new(current_value, current_value_len);

          ok = filterx_list_append(result, &val);

          filterx_object_unref(val);
        }
    }

exit:
  if (!ok)
    {
      filterx_object_unref(result);
    }
  g_list_free_full(cols, (GDestroyNotify)g_free);
  filterx_object_unref(obj);
  csv_scanner_deinit(&scanner);
  return ok?result:NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionParseCSV *self = (FilterXFunctionParseCSV *) s;
  filterx_expr_unref(self->msg);
  filterx_expr_unref(self->columns);
  csv_scanner_options_clean(&self->options);
  filterx_function_free_method(&self->super);
}

static FilterXExpr *
_extract_msg_expr(FilterXFunctionArgs *args, GError **error)
{
  FilterXExpr *msg_expr = filterx_function_args_get_expr(args, 0);
  if (!msg_expr)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be set: msg_str. " FILTERX_FUNC_PARSE_CSV_USAGE);
      return NULL;
    }

  return msg_expr;
}

static FilterXExpr *
_extract_columns_expr(FilterXFunctionArgs *args, GError **error)
{
  return filterx_function_args_get_named_expr(args, FILTERX_FUNC_PARSE_CSV_ARG_NAME_COLUMNS);
}

static gboolean
_extract_opts(FilterXFunctionParseCSV *self, FilterXFunctionArgs *args, GError **error)
{
  guint32 opt_flags = self->options.flags;

  const gchar *error_str = "";
  gboolean exists;
  gsize len;
  const gchar *value;
  FilterXObject *obj;

  value = filterx_function_args_get_named_literal_string(args, FILTERX_FUNC_PARSE_CSV_ARG_NAME_DELIMITERS, &len, &exists);
  if (exists)
    {
      if (len < 1)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_DELIMITERS " can not be empty";
          goto error;
        }
      if (!value)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_DELIMITERS " must be a string literal";
          goto error;
        }
      csv_scanner_options_set_delimiters(&self->options, value);
    }

  value = filterx_function_args_get_named_literal_string(args, FILTERX_FUNC_PARSE_CSV_ARG_NAME_DIALECT, &len, &exists);
  if (exists)
    {
      if (len < 1)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_DIALECT " can not be empty";
          goto error;
        }
      if (!value)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_DIALECT " must be a string literal";
          goto error;
        }
      CSVScannerDialect dialect = csv_parser_lookup_dialect(value);
      if (dialect == -1)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_DIALECT " argument must be one of: [" \
                      "escape-none, " \
                      "escape-backslash, " \
                      "escape-backslash-with-sequences, " \
                      "escape-double-char]";
          goto error;
        }
      csv_scanner_options_set_dialect(&self->options, dialect);
    }

  obj = filterx_function_args_get_named_object(args, FILTERX_FUNC_PARSE_CSV_ARG_NAME_GREEDY, &exists);
  if (exists)
    {
      if (!obj)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_GREEDY " argument evaluation error";
          goto error;
        }
      if (filterx_object_truthy(obj))
        opt_flags |= CSV_SCANNER_GREEDY;
      else
        opt_flags &= ~CSV_SCANNER_GREEDY;
      filterx_object_unref(obj);
    }

  obj = filterx_function_args_get_named_object(args, FILTERX_FUNC_PARSE_CSV_ARG_NAME_STRIP_WHITESPACES, &exists);
  if (exists)
    {
      if (!obj)
        {
          error_str = FILTERX_FUNC_PARSE_CSV_ARG_NAME_STRIP_WHITESPACES " argument evaluation error";
          goto error;
        }
      if (filterx_object_truthy(obj))
        opt_flags |= CSV_SCANNER_GREEDY;
      else
        opt_flags &= ~CSV_SCANNER_GREEDY;
      filterx_object_unref(obj);
    }

  csv_scanner_options_set_flags(&self->options, opt_flags);

  return TRUE;
error:
  g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
              "%s. %s", error_str, FILTERX_FUNC_PARSE_CSV_USAGE);
  return FALSE;
}

static gboolean
_extract_args(FilterXFunctionParseCSV *self, FilterXFunctionArgs *args, GError **error)
{
  gsize args_len = filterx_function_args_len(args);
  if (args_len != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_PARSE_CSV_USAGE);
      return FALSE;
    }

  self->msg = _extract_msg_expr(args, error);
  if (!self->msg)
    return FALSE;

  self->columns = _extract_columns_expr(args, error);

  if (!_extract_opts(self, args, error))
    return FALSE;

  return TRUE;
}

FilterXExpr *
filterx_function_parse_csv_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)

{
  FilterXFunctionParseCSV *self = g_new0(FilterXFunctionParseCSV, 1);
  filterx_function_init_instance(&self->super, function_name);
  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;
  csv_scanner_options_set_delimiters(&self->options, " ");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);

  if (!_extract_args(self, args, error))
    goto error;

  filterx_function_args_free(args);
  return &self->super.super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

gpointer
filterx_function_construct_parse_csv(Plugin *self)
{
  return (gpointer) filterx_function_parse_csv_new;
}
