/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "afuser.h"
#include "messages.h"
#include "compat/getutent.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct _AFUserDestDriver
{
  LogDestDriver super;
  GString *username;
  time_t disable_until;
} AFUserDestDriver;

#ifdef SYSLOG_NG_HAVE_UTMPX_H

typedef struct utmpx UtmpEntry;

static UtmpEntry *
_fetch_utmp_entry(void)
{
  return getutxent();
}

static void
_close_utmp(void)
{
  endutxent();
}

#else

typedef struct utmp UtmpEntry;

static UtmpEntry *
_fetch_utmp_entry(void)
{
  return getutent();
}

static void
_close_utmp(void)
{
  endutent();
}

#endif

static gboolean
_utmp_entry_matches(UtmpEntry *ut, GString *username)
{
#ifdef SYSLOG_NG_HAVE_MODERN_UTMP
  if (ut->ut_type != USER_PROCESS)
    return FALSE;
#endif

  if (strcmp(username->str, "*") == 0)
    return TRUE;

#ifdef SYSLOG_NG_HAVE_MODERN_UTMP
  if (strncmp(username->str, ut->ut_user, sizeof(ut->ut_user)) == 0)
#else
  if (strncmp(username->str, ut->ut_name, sizeof(ut->ut_name)) == 0)
#endif
    return TRUE;

  return FALSE;
}

G_LOCK_DEFINE_STATIC(utmp_lock);

static void
afuser_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFUserDestDriver *self = (AFUserDestDriver *) s;
  gchar buf[8192];
  GString *timestamp;
  UtmpEntry *ut;
  time_t now;

  now = msg->timestamps[LM_TS_RECVD].ut_sec;
  if (self->disable_until && self->disable_until > now)
    goto finish;

  timestamp = g_string_sized_new(0);
  unix_time_format(&msg->timestamps[LM_TS_STAMP], timestamp, TS_FMT_FULL, -1, 0);
  g_snprintf(buf, sizeof(buf), "%s %s %s\n",
             timestamp->str,
             log_msg_get_value(msg, LM_V_HOST, NULL),
             log_msg_get_value(msg, LM_V_MESSAGE, NULL));
  g_string_free(timestamp, TRUE);

  G_LOCK(utmp_lock);
  while ((ut = _fetch_utmp_entry()))
    {
      if (_utmp_entry_matches(ut, self->username))
        {
          gchar line[128];
          gchar *p = line;
          int fd;

          if (ut->ut_line[0] != '/')
            {
              strcpy(line, "/dev/");
              p = line + 5;
            }
          else
            line[0] = 0;
          strncpy(p, ut->ut_line, sizeof(line) - (p - line));
          msg_debug("Posting message to user terminal",
#ifdef SYSLOG_NG_HAVE_MODERN_UTMP
                    evt_tag_str("user", ut->ut_user),
#else
                    evt_tag_str("user", ut->ut_name),
#endif
                    evt_tag_str("line", line));
          fd = open(line, O_NOCTTY | O_APPEND | O_WRONLY | O_NONBLOCK);
          if (fd != -1)
            {
              if (write(fd, buf, strlen(buf)) < 0 && errno == EAGAIN)
                {
                  msg_notice("Writing to the user terminal has blocked for writing, disabling for 10 minutes",
                             evt_tag_str("user", self->username->str));
                  self->disable_until = now + 600;
                }
              close(fd);
            }
        }
    }
  _close_utmp();
  G_UNLOCK(utmp_lock);
finish:
  log_dest_driver_queue_method(s, msg, path_options);
}

void
afuser_dd_free(LogPipe *s)
{
  AFUserDestDriver *self = (AFUserDestDriver *) s;

  g_string_free(self->username, TRUE);
  log_dest_driver_free(s);
}

LogDriver *
afuser_dd_new(gchar *user, GlobalConfig *cfg)
{
  AFUserDestDriver *self = g_new0(AFUserDestDriver, 1);

  log_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.queue = afuser_dd_queue;
  self->super.super.super.free_fn = afuser_dd_free;
  self->username = g_string_new(user);
  return &self->super.super;
}
