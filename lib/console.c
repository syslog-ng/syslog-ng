/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2024 Balázs Scheidler <balazs.scheidler@axoflow.com>
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
#include "console.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>

GMutex console_lock;
gboolean console_present = TRUE;
gboolean using_initial_console = TRUE;
const gchar *console_prefix;

/**
 * console_printf:
 * @fmt: format string
 * @...: arguments to @fmt
 *
 * This function sends a message to the client preferring to use the stderr
 * channel as long as it is available and switching to using syslog() if it
 * isn't. Generally the stderr channell will be available in the startup
 * process and in the beginning of the first startup in the
 * supervisor/daemon processes. Later on the stderr fd will be closed and we
 * have to fall back to using the system log.
 **/
void
console_printf(const gchar *fmt, ...)
{
  gchar buf[2048];
  va_list ap;

  va_start(ap, fmt);
  g_vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (console_is_present())
    fprintf(stderr, "%s: %s\n", console_prefix, buf);
  else
    {
      openlog(console_prefix, LOG_PID, LOG_DAEMON);
      syslog(LOG_CRIT, "%s\n", buf);
      closelog();
    }
}


/* NOTE: this is not synced with any changes and is just an indication whether we have a console */
gboolean
console_is_present(void)
{
  gboolean result;

  /* the lock only serves a memory barrier but is not a real synchronization */
  g_mutex_lock(&console_lock);
  result = console_present;
  g_mutex_unlock(&console_lock);
  return result;
}

gboolean
console_is_attached(void)
{
  gboolean result;
  /* the lock only serves a memory barrier but is not a real synchronization */
  g_mutex_lock(&console_lock);
  if (using_initial_console)
    result = FALSE;
  else
    result = console_present;
  g_mutex_unlock(&console_lock);
  return result;
}

/* re-acquire a console after startup using an array of fds */
gboolean
console_acquire_from_fds(gint fds[3])
{
  const gchar *takeover_message_on_old_console = "[Console taken over, no further output here]\n";
  gboolean result = FALSE;

  g_mutex_lock(&console_lock);
  if (console_present)
    {
      if (!using_initial_console)
        goto exit;
      (void) write(1, takeover_message_on_old_console, strlen(takeover_message_on_old_console));
    }

  dup2(fds[0], STDIN_FILENO);
  dup2(fds[1], STDOUT_FILENO);
  dup2(fds[2], STDERR_FILENO);

  console_present = TRUE;
  using_initial_console = FALSE;
  result = TRUE;
exit:
  g_mutex_unlock(&console_lock);
  return result;
}

/**
 * console_release:
 *
 * Use /dev/null as input/output/error. This function is idempotent, can be
 * called any number of times without harm.
 **/
void
console_release(void)
{
  gint devnull_fd;

  g_mutex_lock(&console_lock);

  if (!console_present)
    goto exit;

  devnull_fd = open("/dev/null", O_RDONLY);
  if (devnull_fd >= 0)
    {
      dup2(devnull_fd, STDIN_FILENO);
      close(devnull_fd);
    }
  devnull_fd = open("/dev/null", O_WRONLY);
  if (devnull_fd >= 0)
    {
      dup2(devnull_fd, STDOUT_FILENO);
      dup2(devnull_fd, STDERR_FILENO);
      close(devnull_fd);
    }
  clearerr(stdin);
  clearerr(stdout);
  clearerr(stderr);
  console_present = FALSE;
  using_initial_console = FALSE;

exit:
  g_mutex_unlock(&console_lock);
}

void
console_global_init(const gchar *console_prefix_)
{
  g_mutex_init(&console_lock);
  console_prefix = console_prefix_;
}

void
console_global_deinit(void)
{
  g_mutex_clear(&console_lock);
}
