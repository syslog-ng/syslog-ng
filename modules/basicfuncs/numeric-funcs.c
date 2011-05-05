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

  if (!tf_parse_int(argv[0]->str, n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg1", argv[0]->str), NULL);
      return FALSE;
    }

  if (!tf_parse_int(argv[1]->str, m))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg1", argv[1]->str), NULL);
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
