static void
tf_echo(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_string_append_len(result, argv[i]->str, argv[i]->len);
      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_echo);


static void
tf_substr(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong start, len;

  /*
   * We need to cast argv[0]->len, which is gsize (so unsigned) type, to a
   * signed type, so we can compare negative offsets/lengths with string
   * length. But if argv[0]->len is bigger than G_MAXLONG, this can lead to
   * completely wrong calculations, so we'll just return nothing (alternative
   * would be to return original string and perhaps print an error...)
   */
  if (argv[0]->len >= G_MAXLONG) {
    msg_error("$(substr) error: string is too long", NULL);
    return;
  }

  /* check number of arguments */
  if (argc < 2 || argc > 3)
    return;

  /* get offset position from second argument */
  if (!tf_parse_int(argv[1]->str, &start)) {
    msg_error("$(substr) parsing failed, start could not be parsed", evt_tag_str("start", argv[1]->str), NULL);
    return;
  }

  /* if we were called with >2 arguments, third was desired length */
  if (argc > 2) {
    if (!tf_parse_int(argv[2]->str, &len)) {
      msg_error("$(substr) parsing failed, length could not be parsed", evt_tag_str("length", argv[2]->str), NULL);
      return;
    }
  } else
    len = (glong)argv[0]->len;

  /*
   * if requested substring length is negative and beyond string start, do nothing;
   * also if it is greater than string length, limit it to string length
   */
  if (len < 0 && -(len) > (glong)argv[0]->len)
    return;
  else if (len > (glong)argv[0]->len)
    len = (glong)argv[0]->len;

  /*
   * if requested offset is beyond string length, do nothing;
   * also, if offset is negative and "before" string beginning, do nothing
   * (or should we perhaps align start with the beginning instead?)
   */
  if (start >= (glong)argv[0]->len)
    return;
  else if (start < 0 && -(start) > (glong)argv[0]->len)
    return;

  /* with negative length, see if we don't end up with start > end */
  if (len < 0 && ((start < 0 && start > len) ||
		  (start >= 0 && (len + ((glong)argv[0]->len) - start) < 0)))
    return;

  /* if requested offset is negative, move start it accordingly */
  if (start < 0) {
    start = start + (glong)argv[0]->len;
    /*
     * this shouldn't actually happen, as earlier we tested for
     * (start < 0 && -start > argv0len), but better safe than sorry
     */
    if (start < 0)
      start = 0;
  }

  /*
   * if requested length is negative, "resize" len to include exactly as many
   * characters as needed from the end of the string, given our start position.
   * (start is always non-negative here already)
   */
  if (len < 0) {
    len = ((glong)argv[0]->len) - start + len;
    /* this also shouldn't happen, but - again - better safe than sorry */
    if (len < 0)
      return;
  }

  /* if we're beyond string end, do nothing */
  if (start >= (glong)argv[0]->len)
    return;

  /* if we want something beyond string end, do it only till the end */
  if (start + len > (glong)argv[0]->len)
    len = ((glong)argv[0]->len) - start;

  /* skip g_string_append_len if we ended up having to extract 0 chars */
  if (len == 0)
    return;

  g_string_append_len(result, argv[0]->str + start, len);
}

TEMPLATE_FUNCTION_SIMPLE(tf_substr);
