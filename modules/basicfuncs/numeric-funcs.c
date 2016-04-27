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

static gboolean
tf_num_parse(gint argc, GString *argv[],
	     const gchar *func_name, gint64 *n, gint64 *m)
{
  if (argc != 2)
    {
      msg_debug("Template function requires two arguments.",
		evt_tag_str("function", func_name));
      return FALSE;
    }

  if (!parse_number_with_suffix(argv[0]->str, n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg1", argv[0]->str));
      return FALSE;
    }

  if (!parse_number_with_suffix(argv[1]->str, m))
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
  gint64 n, m;

  if (!tf_num_parse(argc, argv, "+", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, n + m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_plus);

static void
tf_num_minus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint64 n, m;

  if (!tf_num_parse(argc, argv, "-", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, n - m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_minus);

static void
tf_num_multi(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint64 n, m;

  if (!tf_num_parse(argc, argv, "*", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, n * m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_multi);

static void
tf_num_div(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint64 n, m;

  if (!tf_num_parse(argc, argv, "/", &n, &m) || !m)
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int64_padded(result, 0, ' ', 10, n / m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_div);

static void
tf_num_mod(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint64 n, m;

  if (!tf_num_parse(argc, argv, "%", &n, &m) || !m)
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_uint64_padded(result, 0, ' ', 10, n % m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_mod);


static GString *
_get_gstring_scratch_buffer(const LogTemplateInvokeArgs *args,
                            gsize initial_capacity)
{
  if (args->bufs->len == 0)
    g_ptr_array_add(args->bufs, g_string_sized_new(initial_capacity));

  return (GString *) g_ptr_array_index(args->bufs, 0);
}

static gboolean
_tf_num_parse_arg_with_message(const TFSimpleFuncState *state,
                               LogMessage *message,
                               const LogTemplateInvokeArgs *args,
                               gint64 *number)
{
  GString *formatted_template = _get_gstring_scratch_buffer(args, 64);
  gint on_error = args->opts->on_error;

  log_template_format(state->argv[0], message, args->opts, args->tz,
    args->seq_num, args->context_id, formatted_template);

  if (!parse_number_with_suffix(formatted_template->str, number))
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

static void
tf_num_sum_call(LogTemplateFunction *self, gpointer s,
                const LogTemplateInvokeArgs *args, GString *result)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint64 number;
  gint64 sum = 0;

  gint message_index;
  for (message_index = 0; message_index < args->num_messages; ++message_index)
    {
      LogMessage *message = args->messages[message_index];
      if (_tf_num_parse_arg_with_message(state, message, args, &number))
        sum += number;
    }

  format_int64_padded(result, 0, ' ', 10, sum);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_num_sum,
                  tf_num_prepare, NULL, tf_num_sum_call,
                  tf_simple_func_free_state, NULL);
