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
  gint local_gencounter;
  struct
  {
    struct
    {
      TimeCache buckets[64];
    } localtime;
    struct
    {
      TimeCache buckets[64];
    } gmtime;
    struct
    {
      struct tm key;
      struct tm mutated_key;
      time_t value;
    } mktime;
  } cache;
}
TLS_BLOCK_END;

/* this indicates that a test program is faking the current time */
static gboolean faking_time;

#define current_time_value   __tls_deref(current_time_value)
#define invalidate_time_task __tls_deref(invalidate_time_task)
#define local_gencounter     __tls_deref(local_gencounter)
#define cache                __tls_deref(cache)

static gint timeutils_cache_gencounter = 0;

#if !defined(SYSLOG_NG_HAVE_LOCALTIME_R) || !defined(SYSLOG_NG_HAVE_GMTIME_R)
static GStaticMutex localtime_lock = G_STATIC_MUTEX_INIT;
#endif

static void
_clean_timeutils_cache(void)
{
  memset(&cache.gmtime.buckets, 0, sizeof(cache.gmtime.buckets));
  memset(&cache.localtime.buckets, 0, sizeof(cache.localtime.buckets));
  memset(&cache.mktime.key, 0, sizeof(cache.mktime.key));
}

static void
_validate_timeutils_cache(void)
{
  gint gencounter = g_atomic_int_get(&timeutils_cache_gencounter);

  if (G_UNLIKELY(gencounter != local_gencounter))
    {
      _clean_timeutils_cache();
      local_gencounter = gencounter;
    }
}

void
invalidate_timeutils_cache(void)
{
  tzset();

  g_atomic_int_inc(&timeutils_cache_gencounter);
}

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
  _validate_timeutils_cache();
  if (G_LIKELY(tm->tm_sec == cache.mktime.key.tm_sec &&
               tm->tm_min == cache.mktime.key.tm_min &&
               tm->tm_hour == cache.mktime.key.tm_hour &&
               tm->tm_mday == cache.mktime.key.tm_mday &&
               tm->tm_mon == cache.mktime.key.tm_mon &&
               tm->tm_year == cache.mktime.key.tm_year &&
               tm->tm_isdst == cache.mktime.key.tm_isdst))
    {
      *tm = cache.mktime.mutated_key;
      return cache.mktime.value;
    }

  /* we need to store the incoming value first, as mktime() might change the
   * fields in *tm, for instance in the daylight saving transition hour */
  cache.mktime.key = *tm;
  cache.mktime.value = mktime(tm);

  /* the result we yield consists of both the return value and the mutated
   * key, so we need to save both */
  cache.mktime.mutated_key = *tm;
  return cache.mktime.value;
}

void
cached_localtime(time_t *when, struct tm *tm)
{
  _validate_timeutils_cache();

  guchar i = *when & 0x3F;
  if (G_LIKELY(*when == cache.localtime.buckets[i].when))
    {
      *tm = cache.localtime.buckets[i].tm;
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
      cache.localtime.buckets[i].tm = *tm;
      cache.localtime.buckets[i].when = *when;
    }
}

void
cached_gmtime(time_t *when, struct tm *tm)
{
  _validate_timeutils_cache();

  guchar i = *when & 0x3F;
  if (G_LIKELY(*when == cache.gmtime.buckets[i].when && *when != 0))
    {
      *tm = cache.gmtime.buckets[i].tm;
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
      cache.gmtime.buckets[i].tm = *tm;
      cache.gmtime.buckets[i].when = *when;
    }
}

void timeutils_setup_timezone_hook(void);

static void
timeutils_reset_timezone(gint type, gpointer user_data)
{
  invalidate_timeutils_cache();
  timeutils_setup_timezone_hook();
}

void
timeutils_setup_timezone_hook(void)
{
  register_application_hook(AH_CONFIG_CHANGED, timeutils_reset_timezone, NULL);
}
