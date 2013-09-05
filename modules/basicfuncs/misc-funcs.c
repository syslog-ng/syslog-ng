/*
 * Copyright (c) 2012 BalaBit IT Ltd, Budapest, Hungary
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
tf_context_length_prepare(LogTemplateFunction *self, gpointer s,
                          LogTemplate *parent, gint argc, gchar *argv[],
                          GError **error)
{
  return TRUE;
}

static void
tf_context_length_call(LogTemplateFunction *self, gpointer s,
                       const LogTemplateInvokeArgs *args, GString *result)
{
  g_string_append_printf(result, "%d", args->num_messages);
}

static void
tf_context_length_free_state(gpointer s)
{
}

TEMPLATE_FUNCTION(NULL, tf_context_length,
                  tf_context_length_prepare, NULL, tf_context_length_call,
                  tf_context_length_free_state, NULL);

static void
tf_env(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_string_append(result, getenv(argv[i]->str));
      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_env);
