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
#include "timeutils/zonecache.h"
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
      Cache *zones;
    } tzinfo;
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
  struct
  {
    glong timezone;
    gchar *tzname[2];
  } state;
}
TLS_BLOCK_END;

/* this indicates that a test program is faking the current time */
static gboolean faking_time;

#define current_time_value   __tls_deref(current_time_value)
#define invalidate_time_task __tls_deref(invalidate_time_task)
#define local_gencounter     __tls_deref(local_gencounter)
#define cache                __tls_deref(cache)
#define state                __tls_deref(state)

static struct
{

  /* generation counter for the timezone cache.  To invalidate the thread
   * specific cache, just increase this using g_atomic_int_inc().  Any
   * threads would check if this was changed to see if they need to reset
   * their own cache.  */
  gint cache_gencounter;

  /* copy of the timezone related global variables that might be changed by
   * _any_ localtime() call in any libraries that we use.  We save this
   * information right after a tzset() call, which happens at reload, with
   * our threads not running, i.e.  at a race free point.  Then later on,
   * threads copy these into their own cache in a thread synchronized
   * manner.  */
  glong timezone;
  gchar *tzname[2];
} global_state;


static GMutex localtime_lock;

static glong
_get_system_tzofs(void)
{
#ifdef SYSLOG_NG_HAVE_TIMEZONE
  /* global variable */
  return timezone;
#elif SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF
  time_t t = time(NULL);
  struct tm *tm;

  /* we can't call the cached APIs because the localtime_lock is held, but
   * this is not performance critical.  */
  tm = localtime(&t);
  return -tm->tm_gmtoff;
#else
  return -1;
#endif

}

/* NOTE: this function assumes that localtime_lock() is held */
static void
_capture_timezone_state_from_variables(void)
{
  g_free(global_state.tzname[0]);
  g_free(global_state.tzname[1]);

  /* these are fetching from the tzname & timezone globals.  We cannot use
   * those variables directly as they are managed by the libc functions, and
   * which can be changed in other threads in any localtime() (non _r) call
   * in any of the code of syslog-ng/dependent libraries.
   *
   * This function is only invoked when the global timezone state should be
   * static, as we are either just starting up or being reloaded.
   **/

  global_state.tzname[0] = g_strdup(tzname[0]);
  global_state.tzname[1] = g_strdup(tzname[1]);
  global_state.timezone = _get_system_tzofs();
}

/* NOTE: copy the contents of the global cache to thread local variables */
static void
_copy_timezone_state_to_locals(void)
{
  g_free(state.tzname[0]);
  g_free(state.tzname[1]);

  state.tzname[0] = g_strdup(global_state.tzname[0]);
  state.tzname[1] = g_strdup(global_state.tzname[1]);
  state.timezone = global_state.timezone;
}


static void
_clean_timeutils_cache(void)
{
  memset(&cache.gmtime.buckets, 0, sizeof(cache.gmtime.buckets));
  memset(&cache.localtime.buckets, 0, sizeof(cache.localtime.buckets));
  memset(&cache.mktime.key, 0, sizeof(cache.mktime.key));
  if (cache.tzinfo.zones)
    cache_clear(cache.tzinfo.zones);
}

static void
_validate_timeutils_cache(void)
{
  gint gencounter = g_atomic_int_get(&global_state.cache_gencounter);

  if (G_UNLIKELY(gencounter != local_gencounter))
    {
      _clean_timeutils_cache();

      g_mutex_lock(&localtime_lock);
      _copy_timezone_state_to_locals();
      g_mutex_unlock(&localtime_lock);

      local_gencounter = gencounter;
    }
}

void
invalidate_timeutils_cache(void)
{
  g_mutex_lock(&localtime_lock);
  tzset();
  _capture_timezone_state_from_variables();
  g_mutex_unlock(&localtime_lock);

  g_atomic_int_inc(&global_state.cache_gencounter);
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

      g_mutex_lock(&localtime_lock);
      ltm = localtime(when);
      *tm = *ltm;
      g_mutex_unlock(&localtime_lock);
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

      g_mutex_lock(&localtime_lock);
      ltm = gmtime(when);
      *tm = *ltm;
      g_mutex_unlock(&localtime_lock);
#endif
      cache.gmtime.buckets[i].tm = *tm;
      cache.gmtime.buckets[i].when = *when;
    }
}

TimeZoneInfo *
cached_get_time_zone_info(const gchar *tz)
{
  if (!cache.tzinfo.zones)
    cache.tzinfo.zones = time_zone_cache_new();
  TimeZoneInfo *result = cache_lookup(cache.tzinfo.zones, tz);
  return result;
}

glong
cached_get_system_tzofs(void)
{
  _validate_timeutils_cache();
  return state.timezone;
}

const gchar *const *
cached_get_system_tznames(void)
{
  _validate_timeutils_cache();
  return (const gchar *const *) &state.tzname;
}
