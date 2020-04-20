/*
 * Copyright (c) 2019 Balabit
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "credentials.h"

#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static gchar *credentials_key;
static gchar *credentials_secret;
static gchar **credentials_remaining;

static gchar *
consume_next_from_remaining(gchar **remaining, gint *available_index)
{
  if (!remaining)
    return NULL;

  return remaining[(*available_index)++];
}

static void
set_console_echo(gboolean new_state)
{
  if (!isatty(STDIN_FILENO))
    return;

  struct termios t;

  if (tcgetattr(STDIN_FILENO, &t))
    {
      fprintf(stderr, "syslog-ng-ctl: error while tcgetattr: %s\n", strerror(errno));
      return;
    }

  if (new_state)
    t.c_lflag |= ECHO;
  else
    t.c_lflag &= ~((tcflag_t) ECHO);

  if (tcsetattr(STDIN_FILENO, TCSANOW, &t))
    fprintf(stderr, "syslog-ng-ctl: error while tcsetattr: %s\n", strerror(errno));
}

static void
read_password_from_stdin(gchar *buffer, gsize *length)
{
  printf("enter password:");
  set_console_echo(FALSE);
  if (-1 == getline(&buffer, length, stdin))
    {
      set_console_echo(TRUE);
      fprintf(stderr, "error while reading password from terminal: %s", strerror(errno));
      g_assert_not_reached();
    }
  set_console_echo(TRUE);
  printf("\n");
}

static gint
slng_passwd_add(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gchar *answer;
  gint remaining_unused_index = 0;


  if (!credentials_key)
    credentials_key = consume_next_from_remaining(credentials_remaining, &remaining_unused_index);

  if (!credentials_key)
    {
      gchar *usage = g_option_context_get_help(ctx, TRUE, NULL);
      fprintf(stderr, "Error: missing arguments!\n%s\n", usage);
      g_free(usage);
      return 1;
    }

  if (!is_syslog_ng_running())
    return 1;

  if (!credentials_secret)
    credentials_secret = consume_next_from_remaining(credentials_remaining, &remaining_unused_index);

  gchar *secret_to_store;
  if (credentials_secret)
    {
      secret_to_store = g_strdup(credentials_secret);
      if (!secret_to_store)
        g_assert_not_reached();
    }
  else
    {
      gsize buff_size = 256;
      secret_to_store = g_malloc0(buff_size);
      if (!secret_to_store)
        g_assert_not_reached();

      read_password_from_stdin(secret_to_store, &buff_size);
    }

  gint retval = asprintf(&answer, "PWD %s %s %s", "add", credentials_key, secret_to_store);
  if (retval == -1)
    g_assert_not_reached();

  secret_storage_wipe(secret_to_store, strlen(secret_to_store));
  g_free(secret_to_store);

  if (credentials_secret)
    secret_storage_wipe(credentials_secret, strlen(credentials_secret));

  gint result = dispatch_command(answer);

  secret_storage_wipe(answer, strlen(answer));
  g_free(answer);

  return result;
}

static gint
slng_passwd_status(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gchar *answer;

  gint retval = asprintf(&answer, "PWD %s", "status");
  if (retval == -1)
    g_assert_not_reached();

  return dispatch_command(answer);
}

static GOptionEntry credentials_options_add[] =
{
  { "id", 'i', 0, G_OPTION_ARG_STRING, &credentials_key, "ID of the credential", "<id>" },
  { "secret", 's', 0, G_OPTION_ARG_STRING, &credentials_secret, "Secret part of the credential", "<secret>" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &credentials_remaining, NULL, NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

CommandDescriptor credentials_commands[] =
{
  { "add", credentials_options_add, "Add credentials to credential store", slng_passwd_add },
  { "status", no_options, "Query stored credential status", slng_passwd_status },
  { NULL }
};
