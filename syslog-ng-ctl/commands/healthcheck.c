/*
 * Copyright (c) 2023 László Várady <laszlo.varady93@gmail.com>
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

#include "healthcheck.h"
#include "syslog-ng.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>

static gint healthcheck_options_timeout = 15;

GOptionEntry healthcheck_options[] =
{
  {
    "timeout", 't', 0, G_OPTION_ARG_INT, &healthcheck_options_timeout,
    "maximum seconds to wait for healthcheck results (default: 15)"
  },
  { NULL }
};

static void
_healthcheck_exit(int signo)
{
  _exit(1);
}

static void
_setup_timeout(gint timeout)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = _healthcheck_exit;
  sigaction(SIGALRM, &sa, NULL);

  alarm(timeout);
}

gint
slng_healthcheck(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  _setup_timeout(healthcheck_options_timeout);
  return dispatch_command("HEALTHCHECK");
}
