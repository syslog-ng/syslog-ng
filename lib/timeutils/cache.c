/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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

#include "timeutils/cache.h"
#include "tls-support.h"
#include <apphook.h>

#include <iv.h>
#include <string.h>

typedef struct _TimeCache
{
  time_t when;
  struct tm tm;
} TimeCache;


TLS_BLOCK_START
{
  GTimeVal current_time_value;
  struct iv_task invalidate_time_task;
  TimeCache local_time_cache[64];
  TimeCache gm_time_cache[64];
  struct tm mktime_prev_tm;
  time_t mktime_prev_time;
}
TLS_BLOCK_END;

/* this indicates that a test program is faking the current time */
static gboolean faking_time;

#define current_time_value   __tls_deref(current_time_value)
#define invalidate_time_task __tls_deref(invalidate_time_task)
#define local_time_cache     __tls_deref(local_time_cache)
#define gm_time_cache        __tls_deref(gm_time_cache)
#define mktime_prev_tm       __tls_deref(mktime_prev_tm)
#define mktime_prev_time     __tls_deref(mktime_prev_time)

#if !defined(SYSLOG_NG_HAVE_LOCALTIME_R) || !defined(SYSLOG_NG_HAVE_GMTIME_R)
static GStaticMutex localtime_lock = G_STATIC_MUTEX_INIT;
#endif

void
invalidate_cached_time(void)
{
  current_time_value.tv_sec = 0;
}

void
set_cached_time(GTimeVal *timeval)
{
  current_time_value = *timeval;
  faking_time = TRUE;
}

/*
 * this shuld replace the g_get_current_time and the g_source_get_current_time calls in the main thread
 * (log_msg_init, afinter_postpone_mark)
 */
void
cached_g_current_time(GTimeVal *result)
{
  if (current_time_value.tv_sec == 0)
    {
      g_get_current_time(&current_time_value);
    }
  *result = current_time_value;

  if (G_UNLIKELY(faking_time))
    return;
  else if (iv_inited())
    {
      if (invalidate_time_task.handler == NULL)
        {
          IV_TASK_INIT(&invalidate_time_task);
          invalidate_time_task.handler = (void (*)(void *)) invalidate_cached_time;
        }
      if (!iv_task_registered(&invalidate_time_task))
        iv_task_register(&invalidate_time_task);
    }
  else
    {
      invalidate_cached_time();
    }
}

time_t
cached_g_current_time_sec(void)
{
  GTimeVal now;

  cached_g_current_time(&now);
  return now.tv_sec;
}

time_t
cached_mktime(struct tm *tm)
{
  time_t result;

  if (G_LIKELY(tm->tm_sec == mktime_prev_tm.tm_sec &&
               tm->tm_min == mktime_prev_tm.tm_min &&
               tm->tm_hour == mktime_prev_tm.tm_hour &&
               tm->tm_mday == mktime_prev_tm.tm_mday &&
               tm->tm_mon == mktime_prev_tm.tm_mon &&
               tm->tm_year == mktime_prev_tm.tm_year))
    {
      result = mktime_prev_time;
      return result;
    }
  result = mktime(tm);
  mktime_prev_tm = *tm;
  mktime_prev_time = result;
  return result;
}

void
cached_localtime(time_t *when, struct tm *tm)
{
  guchar i = 0;

  i = *when & 0x3F;
  if (G_LIKELY(*when == local_time_cache[i].when))
    {
      *tm = local_time_cache[i].tm;
      return;
    }
  else
    {
#ifdef SYSLOG_NG_HAVE_LOCALTIME_R
      localtime_r(when, tm);
#else
      struct tm *ltm;

      g_static_mutex_lock(&localtime_lock);
      ltm = localtime(when);
      *tm = *ltm;
      g_static_mutex_unlock(&localtime_lock);
#endif
      local_time_cache[i].tm = *tm;
      local_time_cache[i].when = *when;
    }
}

void
cached_gmtime(time_t *when, struct tm *tm)
{
  guchar i = 0;

  i = *when & 0x3F;
  if (G_LIKELY(*when == gm_time_cache[i].when && *when != 0))
    {
      *tm = gm_time_cache[i].tm;
      return;
    }
  else
    {
#ifdef SYSLOG_NG_HAVE_GMTIME_R
      gmtime_r(when, tm);
#else
      struct tm *ltm;

      g_static_mutex_lock(&localtime_lock);
      ltm = gmtime(when);
      *tm = *ltm;
      g_static_mutex_unlock(&localtime_lock);
#endif
      gm_time_cache[i].tm = *tm;
      gm_time_cache[i].when = *when;
    }
}

void
clean_time_cache(void)
{
  memset(&gm_time_cache, 0, sizeof(gm_time_cache));
  memset(&local_time_cache, 0, sizeof(local_time_cache));
}

void timeutils_setup_timezone_hook(void);

static void
timeutils_reset_timezone(gint type, gpointer context)
{
  tzset();

  // Invalidate time cache to apply time zone as soon as possible.
  invalidate_cached_time();
  clean_time_cache();
  timeutils_setup_timezone_hook();
}

void
timeutils_setup_timezone_hook(void)
{
  register_application_hook(AH_CONFIG_CHANGED, timeutils_reset_timezone, NULL);
}
