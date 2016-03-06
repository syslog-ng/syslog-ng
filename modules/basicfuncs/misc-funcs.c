/*
 * Copyright (c) 2012 Balabit
 * Copyright 2016 Simon Arlott
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

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

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
      char *val = getenv(argv[i]->str);

      if (!val)
        continue;

      g_string_append(result, val);
      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_env);

static void
tf_pwuid_to_nam(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  long buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
  char *buf = g_malloc(buflen);
  struct passwd pwd;
  gint i;

  for (i = 0; i < argc; i++)
    {
      gint64 id;
      struct passwd *pwdp = NULL;

      if (parse_number(argv[i]->str, &id))
        {
           if (getpwuid_r(id, &pwd, buf, buflen, &pwdp) != 0)
             pwdp = NULL;
        }

      if (pwdp)
        {
          g_string_append(result, pwdp->pw_name);
        }
      else
        {
          g_string_append(result, argv[i]->str);
        }

      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }

  g_free(buf);
}

TEMPLATE_FUNCTION_SIMPLE(tf_pwuid_to_nam);

static void
tf_grgid_to_nam(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  long buflen = sysconf(_SC_GETGR_R_SIZE_MAX);
  char *buf = g_malloc(buflen);
  struct group grp;
  gint i;

  for (i = 0; i < argc; i++)
    {
      gint64 id;
      struct group *grpp = NULL;

      if (parse_number(argv[i]->str, &id))
        {
           if (getgrgid_r(id, &grp, buf, buflen, &grpp) != 0)
             grpp = NULL;
        }

      if (grpp)
        {
          g_string_append(result, grpp->gr_name);
        }
      else
        {
          g_string_append(result, argv[i]->str);
        }

      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }

  g_free(buf);
}

TEMPLATE_FUNCTION_SIMPLE(tf_grgid_to_nam);
