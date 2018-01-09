/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2014 Viktor Tusa <tusavik@gmail.com>
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

#include <string.h>
#include <ctype.h>

static void
_append_args_with_separator(gint argc, GString *argv[], GString *result, gchar separator)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_string_append_len(result, argv[i]->str, argv[i]->len);
      if (i < argc - 1)
        g_string_append_c(result, separator);
    }
}


static void
tf_echo(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  _append_args_with_separator(argc, argv, result, ' ');
}

TEMPLATE_FUNCTION_SIMPLE(tf_echo);

static void
tf_length(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      format_uint32_padded(result, 0, 0, 10, argv[i]->len);
      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_length);

/*
 * $(substr $arg START [LEN])
 */
static void
tf_substr(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint64 start, len;

  /*
   * We need to cast argv[0]->len, which is gsize (so unsigned) type, to a
   * signed type, so we can compare negative offsets/lengths with string
   * length. But if argv[0]->len is bigger than G_MAXLONG, this can lead to
   * completely wrong calculations, so we'll just return nothing (alternative
   * would be to return original string and perhaps print an error...)
   */
  if (argv[0]->len >= G_MAXLONG)
    {
      msg_error("$(substr) error: string is too long");
      return;
    }

  /* check number of arguments */
  if (argc < 2 || argc > 3)
    return;

  /* get offset position from second argument */
  if (!parse_number(argv[1]->str, &start))
    {
      msg_error("$(substr) parsing failed, start could not be parsed", evt_tag_str("start", argv[1]->str));
      return;
    }

  /* if we were called with >2 arguments, third was desired length */
  if (argc > 2)
    {
      if (!parse_number(argv[2]->str, &len))
        {
          msg_error("$(substr) parsing failed, length could not be parsed", evt_tag_str("length", argv[2]->str));
          return;
        }
    }
  else
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
  if (start < 0)
    {
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
  if (len < 0)
    {
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

/*
 * $(strip $arg1 $arg2 ...)
 */
static void
tf_strip(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      if (argv[i]->len == 0)
        continue;

      gint spaces_end = 0;
      while (isspace(argv[i]->str[argv[i]->len - spaces_end - 1]) && spaces_end < argv[i]->len)
        spaces_end++;

      if (argv[i]->len == spaces_end)
        continue;

      gint spaces_start = 0;
      while (isspace(argv[i]->str[spaces_start]))
        spaces_start++;

      if (result->len > 0)
        g_string_append_c(result, ' ');

      g_string_append_len(result, &argv[i]->str[spaces_start], argv[i]->len - spaces_end - spaces_start);
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_strip);

/*
 * $(sanitize [opts] $arg1 $arg2 ...)
 *
 * Options:
 *  --ctrl-chars or -c             Filter control characters (default)
 *  --no-ctrl-chars or -C          Don't filter control characters
 *  --invalid-chars <set> or -i    Set of characters to be translated, default "/"
 *  --replacement <replace> or -r  Single character replacement for invalid chars.
 */
typedef struct _TFSanitizeState
{
  TFSimpleFuncState super;
  gint ctrl_chars:1, replacement:8;
  gchar *invalid_chars;
} TFSanitizeState;

static gboolean
tf_sanitize_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                    GError **error)
{
  TFSanitizeState *state = (TFSanitizeState *) s;
  gboolean ctrl_chars = TRUE;
  gchar *invalid_chars = NULL;
  gchar *replacement = NULL;
  GOptionContext *ctx;
  GOptionEntry stize_options[] =
  {
    { "ctrl-chars", 'c', 0, G_OPTION_ARG_NONE, &ctrl_chars, NULL, NULL },
    { "no-ctrl-chars", 'C', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &ctrl_chars, NULL, NULL },
    { "invalid-chars", 'i', 0, G_OPTION_ARG_STRING, &invalid_chars, NULL, NULL },
    { "replacement", 'r', 0, G_OPTION_ARG_STRING, &replacement, NULL, NULL },
    { NULL }
  };
  gboolean result = FALSE;

  ctx = g_option_context_new("sanitize-file");
  g_option_context_add_main_entries(ctx, stize_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      goto exit;
    }
  g_option_context_free(ctx);

  invalid_chars = invalid_chars ? : g_strdup("/");
  replacement = replacement ? : g_strdup("_");

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      goto exit;
    }
  state->ctrl_chars = ctrl_chars;
  state->invalid_chars = g_strdup(invalid_chars);
  state->replacement = replacement[0];
  result = TRUE;

exit:
  /* glib supplies us with duplicated strings that we are responsible for! */
  g_free(invalid_chars);
  g_free(replacement);
  return result;
}

static void
tf_sanitize_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFSanitizeState *state = (TFSanitizeState *) s;
  GString **argv;
  gint argc;
  gint i, pos;

  argv = (GString **) args->bufs->pdata;
  argc = args->bufs->len;
  for (i = 0; i < argc; i++)
    {
      for (pos = 0; pos < argv[i]->len; pos++)
        {
          guchar last_char = argv[i]->str[pos];

          if ((state->ctrl_chars && last_char < 32) ||
              (strchr(state->invalid_chars, (gchar) last_char) != NULL))
            g_string_append_c(result, state->replacement);
          else
            g_string_append_c(result, last_char);
        }
      if (i < argc - 1)
        g_string_append_c(result, '/');
    }
}

static void
tf_sanitize_free_state(gpointer s)
{
  TFSanitizeState *state = (TFSanitizeState *) s;

  g_free(state->invalid_chars);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFSanitizeState, tf_sanitize, tf_sanitize_prepare, tf_simple_func_eval, tf_sanitize_call,
                  tf_sanitize_free_state, NULL);


void
tf_indent_multi_line(LogMessage *msg, gint argc, GString *argv[], GString *text)
{
  gchar *p, *new_line;

  /* append the message text(s) to the template string */
  _append_args_with_separator(argc, argv, text, ' ');

  /* look up the \n-s and insert a \t after them */
  p = text->str;
  new_line = memchr(p, '\n', text->len);
  while(new_line)
    {
      if (*(gchar *)(new_line + 1) != '\t')
        {
          g_string_insert_c(text, new_line - p + 1, '\t');
        }
      new_line = memchr(new_line + 1, '\n', p + text->len - new_line);
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_indent_multi_line);

void
tf_lowercase(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      gchar *new = g_utf8_strdown(argv[i]->str, argv[i]->len);

      g_string_append(result, new);
      if (i < argc - 1)
        g_string_append_c(result, ' ');

      g_free(new);
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_lowercase);

void
tf_uppercase(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      gchar *new = g_utf8_strup(argv[i]->str, argv[i]->len);

      g_string_append(result, new);
      if (i < argc - 1)
        g_string_append_c(result, ' ');

      g_free(new);
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_uppercase);

void
tf_replace_delimiter(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gchar *haystack, *delimiters, new_delimiter;

  if (argc != 3)
    {
      msg_error("$(replace-delimiter) parsing failed, wrong number of arguments");
      return;
    }

  delimiters = argv[0]->str;
  new_delimiter = argv[1]->str[0];
  haystack = g_strdup(argv[2]->str);

  g_string_append(result, g_strdelimit(haystack, delimiters, new_delimiter));

  g_free(haystack);
}

TEMPLATE_FUNCTION_SIMPLE(tf_replace_delimiter);

static void
tf_string_padding(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  GString *text = argv[0];
  GString *padding;
  gint64 width, i;

  if (argc <= 1)
    {
      msg_debug("Not enough arguments for padding template function!");
      return;
    }

  if (!parse_number_with_suffix(argv[1]->str, &width))
    {
      msg_debug("Padding template function requires a number as second argument!");
      return;
    }

  if (argc <= 2)
    padding = g_string_new(" ");
  else
    padding = argv[2];

  if (text->len < width)
    {
      for (i = 0; i < width - text->len; i++)
        {
          g_string_append_c(result, *(padding->str + (i % padding->len)));
        }
    }

  g_string_append_len(result, text->str, text->len);

  if (argc <= 2)
    {
      g_string_free(padding, TRUE);
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_string_padding);

typedef struct _TFBinaryState
{
  TFSimpleFuncState super;
  GString *octets;
} TFBinaryState;

static void
tf_binary_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFBinaryState *state = (TFBinaryState *) s;

  g_string_append_len(result, state->octets->str, state->octets->len);
}

static gboolean
tf_binary_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFBinaryState *state = (TFBinaryState *) s;
  GString *octets = g_string_new("");

  if (argc < 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "$(binary) Incorrect parameters, usage $(binary <number> <number> ...)");
      goto error;
    }

  for (gint i = 1; i < argc; i++)
    {
      gint64 number;

      gchar *token = argv[i];
      if (!parse_number(token, &number))
        {
          g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                      "$(binary) template function requires list of dec/hex/oct numbers as arguments, unable to parse %s as a number",
                      token);
          goto error;
        }
      if (number > 0xFF)
        {
          g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                      "$(binary) template function only supports 8 bit values as characters, %" G_GUINT64_FORMAT " is above 255", number);
          goto error;
        }
      g_string_append_c(octets, (gchar) number);
    }

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      goto error;
    }

  state->octets = octets;
  return TRUE;
error:
  g_string_free(octets, TRUE);
  return FALSE;
}

static void
tf_binary_free_state(gpointer s)
{
  TFBinaryState *state = (TFBinaryState *) s;

  if (state->octets)
    g_string_free(state->octets, TRUE);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFBinaryState, tf_binary, tf_binary_prepare, tf_simple_func_eval, tf_binary_call,
                  tf_binary_free_state, NULL);
