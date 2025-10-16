/*
 * Copyright (c) 2002-2010 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
 * Copyright (c) 2025 One Identity
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

#include "alarms.h"
#include "messages.h"

#ifndef _WIN32
  #include <unistd.h>
  #include <signal.h>
#else
  #include <windows.h>
#endif

#include <string.h>

static gboolean sig_alarm_received = FALSE;
static gboolean alarm_pending = FALSE;

#ifndef _WIN32

static void
sig_alarm_handler(int signo)
{
  sig_alarm_received = TRUE;
}

#else

/* Windows: emulate alarm() with a one-shot timer */
static HANDLE g_alarm_queue = NULL;
static HANDLE g_alarm_timer = NULL;

static VOID CALLBACK
_alarm_callback(PVOID ctx, BOOLEAN fired)
{
  (void)ctx; (void)fired;
  sig_alarm_received = TRUE;
  alarm_pending = FALSE;
}

#endif

void
alarm_set(int timeout)
{
  if (G_UNLIKELY(alarm_pending))
    {
      msg_error("Internal error, alarm_set() called while an alarm is still active");
      return;
    }

#ifndef _WIN32
  alarm(timeout);
#else
  if (!g_alarm_queue)
    g_alarm_queue = CreateTimerQueue();

  /* cancel any stale timer just in case */
  if (g_alarm_timer)
    {
      DeleteTimerQueueTimer(g_alarm_queue, g_alarm_timer, NULL);
      g_alarm_timer = NULL;
    }

  if (timeout > 0)
    {
      /* one-shot after timeout seconds */
      if (CreateTimerQueueTimer(&g_alarm_timer, g_alarm_queue,
                                _alarm_callback, NULL,
                                (DWORD)timeout * 1000, 0, WT_EXECUTEDEFAULT))
        {
          /* ok */
        }
      else
        {
          msg_error("alarm_set(): failed to create timer", evt_tag_int("timeout", timeout));
          return;
        }
    }
#endif
  alarm_pending = TRUE;
}

void
alarm_cancel(void)
{
#ifndef _WIN32
  alarm(0);
#else
  if (g_alarm_timer)
    {
      DeleteTimerQueueTimer(g_alarm_queue, g_alarm_timer, NULL);
      g_alarm_timer = NULL;
    }
#endif
  sig_alarm_received = FALSE;
  alarm_pending = FALSE;
}

gboolean
alarm_has_fired(void)
{
  gboolean res = sig_alarm_received;
  return res;
}

void
alarm_init(void)
{
#ifndef _WIN32
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_alarm_handler;
  sigaction(SIGALRM, &sa, NULL);
#else
  if (!g_alarm_queue)
    g_alarm_queue = CreateTimerQueue();
#endif
}
