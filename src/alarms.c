/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "alarms.h"
#include "messages.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>

static gboolean sig_alarm_received = FALSE;
static gboolean alarm_pending = FALSE;

static void
sig_alarm_handler(int signo)
{
  sig_alarm_received = TRUE;
}

void
alarm_set(int timeout)
{
  if (G_UNLIKELY(alarm_pending))
    {
      msg_error("Internal error, alarm_set() called while an alarm is still active", NULL);
      return;
    }
  alarm(timeout);
  alarm_pending = TRUE;
}

void
alarm_cancel()
{
  alarm(0);
  sig_alarm_received = FALSE;
  alarm_pending = FALSE;
}

gboolean
alarm_has_fired()
{
  gboolean res = sig_alarm_received;
  return res;
}

void
alarm_init()
{
  struct sigaction sa;
  
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_alarm_handler;
  sigaction(SIGALRM, &sa, NULL);
}
