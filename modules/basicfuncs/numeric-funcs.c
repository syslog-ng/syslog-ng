/*
 * Copyright (c) 2002-2014 BalaBit IT Ltd, Budapest, Hungary
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
	     const gchar *func_name, glong *n, glong *m)
{
  if (argc != 2)
    {
      msg_debug("Template function requires two arguments.",
		evt_tag_str("function", func_name), NULL);
      return FALSE;
    }

  if (!parse_number_with_suffix(argv[0]->str, n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg1", argv[0]->str), NULL);
      return FALSE;
    }

  if (!parse_number_with_suffix(argv[1]->str, m))
    {
      msg_debug("Parsing failed, template function's second argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg2", argv[1]->str), NULL);
      return FALSE;
    }

  return TRUE;
}

static void
tf_num_plus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "+", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n + m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_plus);

static void
tf_num_minus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "-", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n - m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_minus);

static void
tf_num_multi(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "*", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n * m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_multi);

static void
tf_num_div(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "/", &n, &m) || !m)
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n / m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_div);

static void
tf_num_mod(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "%", &n, &m) || !m)
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_uint32_padded(result, 0, ' ', 10, n % m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_mod);
