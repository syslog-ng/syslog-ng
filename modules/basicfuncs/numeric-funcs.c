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
} Number;

gdouble
number_as_double(Number number)
{
  if (number.value_type == Float)
    return number.value_data.raw_float;

  return number.value_data.raw_integer;
}

gint64
number_as_int(Number number)
{
  if (number.value_type == Integer)
    return number.value_data.raw_integer;

  return number.value_data.raw_float;
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
  if (parse_dec_number(str, &(number->value_data.raw_integer)))
    {
      number->value_type = Integer;
      return TRUE;
    }

  if (parse_float(str, &(number->value_data.raw_float)))
    {
      number->value_type = Float;
      return TRUE;
    }

  return FALSE;
}

void
format_number(GString *result, Number n)
{
  if (n.value_type == Integer)
    {
      format_int64_padded(result, 0, ' ', 10, number_as_int(n));
      return;
    }

  g_string_append_printf(result, "%.20f", number_as_double(n));
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
tf_num_plus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "+", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      res.value_type = Integer;
      res.value_data.raw_integer = number_as_int(n) + number_as_int(m);
    }
  else
    {
      res.value_type = Float;
      res.value_data.raw_float = number_as_double(n) + number_as_double(m);
    }

  format_number(result, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_plus);

static void
tf_num_minus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "-", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      res.value_type = Integer;
      res.value_data.raw_integer = number_as_int(n) - number_as_int(m);
    }
  else
    {
      res.value_type = Float;
      res.value_data.raw_float = number_as_double(n) - number_as_double(m);
    }

  format_number(result, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_minus);

static void
tf_num_multi(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "*", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      res.value_type = Integer;
      res.value_data.raw_integer = number_as_int(n) * number_as_int(m);
    }
  else
    {
      res.value_type = Float;
      res.value_data.raw_float = number_as_double(n) * number_as_double(m);
    }

  format_number(result, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_multi);

static void
tf_num_div(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "/", &n, &m) || number_is_zero(m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      res.value_type = Integer;
      res.value_data.raw_integer = number_as_int(n) / number_as_int(m);
    }
  else
    {
      res.value_type = Float;
      res.value_data.raw_float = number_as_double(n) / number_as_double(m);
    }

  format_number(result, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_div);

static void
tf_num_mod(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n, m, res;

  if (!tf_num_parse(argc, argv, "%", &n, &m) || number_is_zero(m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (n.value_type == Integer && m.value_type == Integer)
    {
      res.value_type = Integer;
      res.value_data.raw_integer = number_as_int(n) % number_as_int(m);
    }
  else
    {
      res.value_type = Float;
      res.value_data.raw_float = fmod(number_as_double(n), number_as_double(m));
    }

  format_number(result, res);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_mod);

static void
tf_num_round(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n;

  if (argc != 1)
    {
      msg_debug("Template function requires one argument.",
                evt_tag_str("function", "round"));
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (!parse_integer_or_float(argv[0]->str, &n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", "round"),
                evt_tag_str("arg1", argv[0]->str));
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, round(number_as_double(n)));
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_round);

static void
tf_num_ceil(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n;

  if (argc != 1)
    {
      msg_debug("Template function requires one argument.",
                evt_tag_str("function", "ceil"));
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (!parse_integer_or_float(argv[0]->str, &n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", "ceil"),
                evt_tag_str("arg1", argv[0]->str));
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, ceil(number_as_double(n)));
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_ceil);

static void
tf_num_floor(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  Number n;

  if (argc != 1)
    {
      msg_debug("Template function requires one argument.",
                evt_tag_str("function", "floor"));
      g_string_append_len(result, "NaN", 3);
      return;
    }

  if (!parse_integer_or_float(argv[0]->str, &n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
                evt_tag_str("function", "floor"),
                evt_tag_str("arg1", argv[0]->str));
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, floor(number_as_double(n)));
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_floor);

static gboolean
_tf_num_parse_arg_with_message(const TFSimpleFuncState *state,
                               LogMessage *message,
                               const LogTemplateInvokeArgs *args,
                               gint64 *number)
{
  GString *formatted_template = scratch_buffers_alloc();
  gint on_error = args->opts->on_error;

  log_template_format(state->argv_templates[0], message, args->opts, args->tz,
                      args->seq_num, args->context_id, formatted_template);

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
                    AggregateFunc aggregate, GString *result)
{
  gint64 accumulator;
  if (!_tf_num_filter(state, args, _tf_num_store_first, aggregate, &accumulator))
    return;

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
                const LogTemplateInvokeArgs *args, GString *result)
{
  _tf_num_aggregation((TFSimpleFuncState *) s, args, _tf_num_sum, result);
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
                const LogTemplateInvokeArgs *args, GString *result)
{
  _tf_num_aggregation((TFSimpleFuncState *) s, args, _tf_num_minimum, result);
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
                const LogTemplateInvokeArgs *args, GString *result)
{
  _tf_num_aggregation((TFSimpleFuncState *) s, args, _tf_num_maximum, result);
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
                    const LogTemplateInvokeArgs *args, GString *result)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *)s;
  AverageState accumulator = {0, 0};

  if (!_tf_num_filter(state, args, _tf_num_store_average_first, _tf_num_average, &accumulator))
    return;

  g_assert(accumulator.count > 0);
  gint64 mean = accumulator.sum / accumulator.count;
  format_int64_padded(result, 0, ' ', 10, mean);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_num_average,
                  tf_num_prepare, NULL, tf_num_average_call,
                  tf_simple_func_free_state, NULL);
