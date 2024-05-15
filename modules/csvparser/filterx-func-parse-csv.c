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


typedef struct FilterXFunctionParseCSV_
{
  FilterXFunction super;
  FilterXExpr *msg;
  CSVScannerOptions options;
  FilterXExpr *columns;
} FilterXFunctionParseCSV;

#define STRINGIFY(lit) #lit
#define NUM_MANDATORY_ARGS 1

typedef enum FxFnParseCSVOpt_
{
  FxFnParseCSVOptColumns = NUM_MANDATORY_ARGS,
  FxFnParseCSVOptDelimiters,
  FxFnParseCSVOptDialect,
  FxFnParseCSVOptGreedy,
  FxFnParseCSVOptStripWhiteSpace,

  FxFnParseCSVOptFirst = FxFnParseCSVOptDelimiters, // workaround, since columns need to parse on a different way yet (orig val: FxFnParseCSVOptColumns)
  FxFnParseCSVOptLast = FxFnParseCSVOptStripWhiteSpace
} FxFnParseCSVOpt;

struct ArgumentDescriptor
{
  const gchar *name;
  FilterXType *acceptable_type;
};

static const struct ArgumentDescriptor args_descs[FxFnParseCSVOptLast - FxFnParseCSVOptFirst + 1] =
{
  // {
  //   .name = "columns",
  //   .acceptable_type = &FILTERX_TYPE_NAME(json_array),
  // },
  {
    .name = "delimiters",
    .acceptable_type = &FILTERX_TYPE_NAME(string),
  },
  {
    .name = "dialect",
    .acceptable_type = &FILTERX_TYPE_NAME(string),
  },
  {
    .name = "greedy",
    .acceptable_type = &FILTERX_TYPE_NAME(boolean),
  },
  {
    .name = "strip_whitespaces",
    .acceptable_type = &FILTERX_TYPE_NAME(boolean),
  },
};

#define NUM_CVS_SCANNER_DIALECTS 4

static const gchar *parse_csv_dialect_enum_names[NUM_CVS_SCANNER_DIALECTS] =
{
  STRINGIFY(CSV_SCANNER_ESCAPE_NONE),
  STRINGIFY(CSV_SCANNER_ESCAPE_BACKSLASH),
  STRINGIFY(CSV_SCANNER_ESCAPE_BACKSLASH_WITH_SEQUENCES),
  STRINGIFY(CSV_SCANNER_ESCAPE_DOUBLE_CHAR),
};

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
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(message_value)))
    input = filterx_message_value_get_value(obj, &len);
  else
    goto exit;

  FilterXObject *result = NULL;
  GList *cols = NULL;
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
_extract_parse_csv_msg_expr(GList *argument_expressions, GError **error)
{
  FilterXExpr *msg_expr = filterx_expr_ref(((FilterXExpr *) argument_expressions->data));
  if (!msg_expr)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be set: message. " FILTERX_FUNC_PARSE_CSV_USAGE);
      return NULL;
    }

  return msg_expr;
}

static FilterXExpr *
_extract_parse_csv_columns_expr(GList *argument_expressions, GError **error)
{
  gsize arguments_len = argument_expressions ? g_list_length(argument_expressions) : 0;
  if (arguments_len - NUM_MANDATORY_ARGS >= FxFnParseCSVOptColumns)
    {
      return filterx_expr_ref((FilterXExpr *) g_list_nth_data(argument_expressions, FxFnParseCSVOptColumns));
    }
  return NULL;
}

static gboolean
_extract_parse_csv_opts(FilterXFunctionParseCSV *self, GList *argument_expressions, GError **error)
{
  gsize arguments_len = argument_expressions ? g_list_length(argument_expressions) : 0;
  if (arguments_len < NUM_MANDATORY_ARGS)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_PARSE_CSV_USAGE);
      return FALSE;
    }

  guint32 opt_flags = self->options.flags;

  FilterXObject *arg_obj = NULL;
  int opt_id = FxFnParseCSVOptFirst;
  for (GList *elem = g_list_nth(argument_expressions, opt_id); elem; elem = elem->next)
    {
      if (opt_id > FxFnParseCSVOptLast)
        break;

      FilterXExpr *argument_expr = (FilterXExpr *)elem->data;
      if (!argument_expr || !filterx_expr_is_literal(argument_expr))
        {
          g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                      "'%s' argument must be string literal. " FILTERX_FUNC_PARSE_CSV_USAGE, args_descs[opt_id - FxFnParseCSVOptFirst].name);
          return FALSE;
        }

      arg_obj = filterx_expr_eval(argument_expr);
      if (!arg_obj)
        {
          g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                      "unable to parse argument '%s'. " FILTERX_FUNC_PARSE_CSV_USAGE, args_descs[opt_id - FxFnParseCSVOptFirst].name);
          return FALSE;
        }

      // optional args must be nullable
      if (filterx_object_is_type(arg_obj, &FILTERX_TYPE_NAME(null)))
        goto next;


      const gchar *opt_str = NULL;
      gboolean opt_bool = FALSE;
      if (!filterx_object_is_type(arg_obj, args_descs[opt_id - FxFnParseCSVOptFirst].acceptable_type))
        {
          g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                      "'%s' argument must be string literal " FILTERX_FUNC_PARSE_CSV_USAGE, args_descs[opt_id - FxFnParseCSVOptFirst].name);
          goto error;
        }
      if (filterx_object_is_type(arg_obj, &FILTERX_TYPE_NAME(string)))
        {
          opt_str = filterx_string_get_value(arg_obj, NULL);
          if (!opt_str)
            {
              g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                          "'%s' argument must be string literal " FILTERX_FUNC_PARSE_CSV_USAGE, args_descs[opt_id - FxFnParseCSVOptFirst].name);
              goto error;
            }
        }
      else if (filterx_object_is_type(arg_obj, &FILTERX_TYPE_NAME(boolean)))
        opt_bool = filterx_object_truthy(arg_obj);


      switch (opt_id)
        {
        case FxFnParseCSVOptColumns:
          // this should not happened
          // the framework is not yet able to handle lists/dicts on parse phase
          // do nothing, parse columns in eval temporary
          break;
        case FxFnParseCSVOptDelimiters:
          csv_scanner_options_set_delimiters(&self->options, opt_str);
          break;
        case FxFnParseCSVOptDialect:
        {
          CSVScannerDialect dialect = -1;
          for (int e = 0; e < NUM_CVS_SCANNER_DIALECTS; e++)
            {
              if (strcmp(parse_csv_dialect_enum_names[e], opt_str) == 0)
                {
                  dialect = e;
                  break;
                }
            }
          if (dialect != -1)
            csv_scanner_options_set_dialect(&self->options, dialect);
          break;
        }
        case FxFnParseCSVOptGreedy:
          if (opt_bool)
            opt_flags |= CSV_SCANNER_GREEDY;
          else
            opt_flags &= ~CSV_SCANNER_GREEDY;
          break;
        case FxFnParseCSVOptStripWhiteSpace:
          if (opt_bool)
            opt_flags |= CSV_SCANNER_STRIP_WHITESPACE;
          else
            opt_flags &= ~CSV_SCANNER_STRIP_WHITESPACE;
          break;
        default:
          g_assert_not_reached();
          break;
        }
next:
      filterx_object_unref(arg_obj);
      opt_id++;
    }

  csv_scanner_options_set_flags(&self->options, opt_flags);

  return TRUE;
error:
  filterx_object_unref(arg_obj);
  return FALSE;
}

FilterXExpr *
filterx_function_parse_csv_new(const gchar *function_name, GList *argument_expressions, GError **error)

{
  FilterXFunctionParseCSV *self = g_new0(FilterXFunctionParseCSV, 1);
  filterx_function_init_instance(&self->super, function_name);
  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;
  csv_scanner_options_set_delimiters(&self->options, " ");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);

  // there is no filterx context in construct phase, unable to parse json variable
  // and json literals are not yet supported
  if (!_extract_parse_csv_opts(self, argument_expressions, error))
    goto error;

  self->columns = _extract_parse_csv_columns_expr(argument_expressions, error);

  self->msg = _extract_parse_csv_msg_expr(argument_expressions, error);
  if (!self->msg)
    goto error;

  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  return &self->super.super;

error:
  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

gpointer
filterx_function_construct_parse_csv(Plugin *self)
{
  return (gpointer) filterx_function_parse_csv_new;
}
