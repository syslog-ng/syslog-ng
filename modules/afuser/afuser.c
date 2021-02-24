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
#include "timeutils/format.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct _AFUserDestDriver
{
  LogDestDriver super;
  GString *username;
  time_t disable_until;
  time_t time_reopen;
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

static gchar *
_get_utmp_username(UtmpEntry *ut)
{
#ifdef SYSLOG_NG_HAVE_MODERN_UTMP
  return ut->ut_user;
#else
  return ut->ut_name;
#endif
}

G_LOCK_DEFINE_STATIC(utmp_lock);

void
afuser_dd_set_time_reopen(LogDriver *s, time_t time_reopen)
{
  AFUserDestDriver *self = (AFUserDestDriver *) s;

  self->time_reopen = time_reopen;
}

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
  format_unix_time(&msg->timestamps[LM_TS_STAMP], timestamp, TS_FMT_FULL, -1, 0);
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
          strncpy(p, ut->ut_line, MIN(sizeof(ut->ut_line), sizeof(line) - (p - line)));
          msg_debug("Posting message to user terminal",
                    evt_tag_str("user", _get_utmp_username(ut)),
                    evt_tag_str("line", line));
          fd = open(line, O_NOCTTY | O_APPEND | O_WRONLY | O_NONBLOCK);
          if (fd == -1)
            {
              msg_error("Opening tty device failed, disabling usertty() for time-reopen seconds",
                        evt_tag_str("user", _get_utmp_username(ut)),
                        evt_tag_str("line", line),
                        evt_tag_int("time_reopen", self->time_reopen),
                        evt_tag_error("errno"));
              self->disable_until = now + self->time_reopen;
              continue;
            }
          if (write(fd, buf, strlen(buf)) < 0 && errno == EAGAIN)
            {
              msg_notice("Writing to the user terminal has blocked for writing, disabling for time-reopen seconds",
                         evt_tag_str("user", self->username->str),
                         evt_tag_int("time_reopen", self->time_reopen));
              self->disable_until = now + self->time_reopen;
            }
          close(fd);
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

static gboolean
afuser_dd_init(LogPipe *s)
{
  AFUserDestDriver *self = (AFUserDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->time_reopen == -1)
    self->time_reopen = cfg->time_reopen;

  return log_dest_driver_init_method(s);
}

LogDriver *
afuser_dd_new(gchar *user, GlobalConfig *cfg)
{
  AFUserDestDriver *self = g_new0(AFUserDestDriver, 1);

  log_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.init = afuser_dd_init;
  self->super.super.super.queue = afuser_dd_queue;
  self->super.super.super.free_fn = afuser_dd_free;
  self->username = g_string_new(user);
  self->time_reopen = -1;

  return &self->super.super;
}
