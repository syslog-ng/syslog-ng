/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include <math.h>

typedef gboolean (*AggregateFunc)(gpointer, gint64);

typedef struct _Number
{
  enum
  {
    Integer,
    Float
  } value_type;
  union
  {
    gint64 raw_integer;
    gdouble raw_float;
  } value_data;
  int precision;
} Number;

gdouble
number_as_double(Number number)
{
  if (number.value_type == Float)
    return number.value_data.raw_float;

  return number.value_data.raw_integer;
}

void
number_set_double(Number *number, double value)
{
  number->value_type = Float;
  number->value_data.raw_float = value;
  number->precision = 20;
}

gint64
number_as_int(Number number)
{
  if (number.value_type == Integer)
    return number.value_data.raw_integer;

  return (gint64) number.value_data.raw_float;
}

void
number_set_int(Number *number, gint64 value)
{
  number->value_type = Integer;
  number->value_data.raw_integer = value;
  number->precision = 0;
}

gboolean
number_is_zero(Number number)
{
  if (number.value_type == Integer)
    return number.value_data.raw_integer == 0;

  return fabs(number.value_data.raw_float) < DBL_EPSILON;
}

gboolean
parse_integer_or_float(const char *str, Number *number)
{
  gint64 int_value;
  if (parse_dec_number(str, &int_value))
    {
      number_set_int(number, int_value);
      return TRUE;
    }

  double float_value;
  if (parse_float(str, &float_value))
    {
      number_set_double(number, float_value);
      return TRUE;
    }

  return FALSE;
}

void
format_number(GString *result, LogMessageValueType *type, Number n)
{
  if (n.value_type == Integer)
    {
      *type = LM_VT_INT64;
      format_int64_padded(result, 0, ' ', 10, number_as_int(n));
      return;
    }

  *type = LM_VT_DOUBLE;
  g_string_append_printf(result, "%.*f", n.precision, number_as_double(n));
}

void
format_nan(GString *result, LogMessageValueType *type)
{
  g_string_append_len(result, "NaN", 3);
  *type = LM_VT_STRING;
}

static gboolean
tf_num_parse(gint argc, GString *argv[],
             const gchar *func_name, Number *n, Number *m)
{
  if (argc != 2)
    {
      msg_debug("Template function requires two arguments.",
                evt_tag_str("function", func_name));
      return FALSE;
    }

  if (!parse_integer_or_float(argv[0]->str, n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", func_name),
                evt_tag_str("arg1", argv[0]->str));
      return FALSE;
    }

  if (!parse_integer_or_float(argv[1]->str, m))
    {
      msg_debug("Parsing failed, template function's second argument is not a number",
                evt_tag_str("function", func_name),
                evt_tag_str("arg2", argv[1]->str));
      return FALSE;
    }

  return TRUE;
}

static void
tf_num_plus(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "+", &n, &m))
    {
      format_nan(result, type);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      number_set_int(&res, number_as_int(n) + number_as_int(m));
    }
  else
    {
      number_set_double(&res, number_as_double(n) + number_as_double(m));
    }

  format_number(result, type, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_plus);

static void
tf_num_minus(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "-", &n, &m))
    {
      format_nan(result, type);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      number_set_int(&res, number_as_int(n) - number_as_int(m));
    }
  else
    {
      number_set_double(&res, number_as_double(n) - number_as_double(m));
    }

  format_number(result, type, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_minus);

static void
tf_num_multi(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "*", &n, &m))
    {
      format_nan(result, type);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      number_set_int(&res, number_as_int(n) * number_as_int(m));
    }
  else
    {
      number_set_double(&res, number_as_double(n) * number_as_double(m));
    }

  format_number(result, type, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_multi);

static void
tf_num_div(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "/", &n, &m) || number_is_zero(m))
    {
      format_nan(result, type);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      number_set_int(&res, number_as_int(n) / number_as_int(m));
    }
  else
    {
      number_set_double(&res, number_as_double(n) / number_as_double(m));
    }

  format_number(result, type, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_div);

static void
tf_num_mod(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "%", &n, &m) || number_is_zero(m))
    {
      format_nan(result, type);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      number_set_int(&res, number_as_int(n) % number_as_int(m));
    }
  else
    {
      number_set_double(&res, fmod(number_as_double(n), number_as_double(m)));
    }

  format_number(result, type, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_mod);

static void
tf_num_round(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n;
  gint64 precision = 0;

  if (argc < 1 || argc > 2)
    {
      msg_debug("Template function requires exactly one or two arguments.",
                evt_tag_str("function", "round"));
      format_nan(result, type);
      return;
    }

  if (!parse_integer_or_float(argv[0]->str, &n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", "round"),
                evt_tag_str("arg1", argv[0]->str));
      format_nan(result, type);
      return;
    }

  if (argc > 1)
    {
      if (!parse_dec_number(argv[1]->str, &precision))
        {
          msg_debug("Parsing failed, template function's second argument is not a number",
                    evt_tag_str("function", "round"),
                    evt_tag_str("arg2", argv[1]->str));
          format_nan(result, type);
          return;
        }

      if (precision < 0 || precision > 20)
        {
          msg_debug("Parsing failed, precision is not in the supported range (0..20)",
                    evt_tag_str("function", "round"),
                    evt_tag_str("arg2", argv[1]->str));
          format_nan(result, type);
          return;
        }
    }

  double multiplier = pow(10, precision);
  double res = round(number_as_double(n) * multiplier) / multiplier;
  number_set_double(&n, res);

  /*
   * number_set_double() resets the precision, so assign it now.
   */
  n.precision = precision;

  format_number(result, type, n);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_round);

static void
tf_num_ceil(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n;

  if (argc != 1)
    {
      msg_debug("Template function requires one argument.",
                evt_tag_str("function", "ceil"));
      format_nan(result, type);
      return;
    }

  if (!parse_integer_or_float(argv[0]->str, &n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", "ceil"),
                evt_tag_str("arg1", argv[0]->str));
      format_nan(result, type);
      return;
    }

  *type = LM_VT_INT64;
  gdouble number=ceil(number_as_double(n))
  format_int64_padded(result, 0, ' ', 10, (gint64)number);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_ceil);

static void
tf_num_floor(LogMessage *msg, gint argc, GString *argv[], GString *result, LogMessageValueType *type)
{
  Number n;

  if (argc != 1)
    {
      msg_debug("Template function requires one argument.",
                evt_tag_str("function", "floor"));
      format_nan(result, type);
      return;
    }

  if (!parse_integer_or_float(argv[0]->str, &n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", "floor"),
                evt_tag_str("arg1", argv[0]->str));
      format_nan(result, type);
      return;
    }

  *type = LM_VT_INT64;
  format_int64_padded(result, 0, ' ', 10, (gint64)floor(number_as_double(n)));
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_floor);

static gboolean
_tf_num_parse_arg_with_message(const TFSimpleFuncState *state,
                               LogMessage *message,
                               const LogTemplateInvokeArgs *args,
                               gint64 *number)
{
  GString *formatted_template = scratch_buffers_alloc();
  gint on_error = args->options->opts->on_error;

  log_template_format(state->argv_templates[0], message, args->options, formatted_template);

  if (!parse_dec_number(formatted_template->str, number))
    {
      if (!(on_error & ON_ERROR_SILENT))
        msg_error("Parsing failed, template function's argument is not a number",
                  evt_tag_str("arg", formatted_template->str));
      return FALSE;
    }

  return TRUE;
}

static gboolean
tf_num_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
               gint argc, gchar *argv[], GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (argc != 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "$(%s) requires only one argument", argv[0]);
      return FALSE;
    }

  return tf_simple_func_prepare(self, s, parent, argc, argv, error);
}

static gint
_tf_num_filter_iterate(const TFSimpleFuncState *state,
                       const LogTemplateInvokeArgs *args,
                       gint message_index,
                       AggregateFunc aggregate,
                       gpointer accumulator)
{
  for (; message_index < args->num_messages; message_index++)
    {
      LogMessage *message = args->messages[message_index];
      gint64 number;
      if ((_tf_num_parse_arg_with_message(state, message, args, &number) &&
           (!aggregate(accumulator, number))))
        return message_index;
    }

  return -1;
}

static gboolean
_tf_num_filter(const TFSimpleFuncState *state,
               const LogTemplateInvokeArgs *args,
               AggregateFunc start,
               AggregateFunc aggregate,
               gpointer accumulator)
{
  gint first = _tf_num_filter_iterate(state, args, 0, start, accumulator);
  if (first < 0)
    return FALSE;

  _tf_num_filter_iterate(state, args, first + 1, aggregate, accumulator);
  return TRUE;
}

static gboolean
_tf_num_store_first(gpointer accumulator, gint64 element)
{
  gint64 *acc = (gint64 *)accumulator;
  *acc = element;
  return FALSE;
}

static void
_tf_num_aggregation(TFSimpleFuncState *state, const LogTemplateInvokeArgs *args,
                    AggregateFunc aggregate, GString *result, LogMessageValueType *type)
{
  gint64 accumulator;

  if (!_tf_num_filter(state, args, _tf_num_store_first, aggregate, &accumulator))
    {
      /* invalid arguments, empty string */
      *type = LM_VT_NULL;
      return;
    }

  *type = LM_VT_INT64;
  format_int64_padded(result, 0, ' ', 10, accumulator);
}

static gboolean
_tf_num_sum(gpointer accumulator, gint64 element)
{
  gint64 *acc = (gint64 *)accumulator;
  *acc += element;
  return TRUE;
}

static void
tf_num_sum_call(LogTemplateFunction *self, gpointer s,
                const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  _tf_num_aggregation((TFSimpleFuncState *) s, args, _tf_num_sum, result, type);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_num_sum,
                  tf_num_prepare, NULL, tf_num_sum_call,
                  tf_simple_func_free_state, NULL);

static gboolean
_tf_num_minimum(gpointer accumulator, gint64 element)
{
  gint64 *acc = (gint64 *)accumulator;
  if (element < *acc)
    *acc = element;

  return TRUE;
}

static void
tf_num_min_call(LogTemplateFunction *self, gpointer s,
                const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  _tf_num_aggregation((TFSimpleFuncState *) s, args, _tf_num_minimum, result, type);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_num_min,
                  tf_num_prepare, NULL, tf_num_min_call,
                  tf_simple_func_free_state, NULL);

static gboolean
_tf_num_maximum(gpointer accumulator, gint64 element)
{
  gint64 *acc = (gint64 *)accumulator;
  if (element > *acc)
    *acc = element;

  return TRUE;
}

static void
tf_num_max_call(LogTemplateFunction *self, gpointer s,
                const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  _tf_num_aggregation((TFSimpleFuncState *) s, args, _tf_num_maximum, result, type);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_num_max,
                  tf_num_prepare, NULL, tf_num_max_call,
                  tf_simple_func_free_state, NULL);

typedef struct _AverageState
{
  gint count;
  gint64 sum;
} AverageState;

static gboolean
_tf_num_store_average_first(gpointer accumulator, gint64 element)
{
  AverageState *state = (AverageState *) accumulator;
  state->count = 1;
  state->sum = element;
  return FALSE;
}

static gboolean
_tf_num_average(gpointer accumulator, gint64 element)
{
  AverageState *state = (AverageState *) accumulator;
  ++state->count;
  state->sum += element;
  return TRUE;
}

static void
tf_num_average_call(LogTemplateFunction *self, gpointer s,
                    const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *)s;
  AverageState accumulator = {0, 0};

  if (!_tf_num_filter(state, args, _tf_num_store_average_first, _tf_num_average, &accumulator))
    {
      *type = LM_VT_NULL;
      return;
    }

  /* _tf_num_filter() would return FALSE if there are no elements, so
   * handling this case with assert is fine */

  g_assert(accumulator.count > 0);
  *type = LM_VT_INT64;
  gint64 mean = accumulator.sum / accumulator.count;
  format_int64_padded(result, 0, ' ', 10, mean);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_num_average,
                  tf_num_prepare, NULL, tf_num_average_call,
                  tf_simple_func_free_state, NULL);
