/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "timeutils/timeutils.h"
#include "messages.h"
#include "tls-support.h"

#include <string.h>
#include <iv.h>

const char *month_names_abbrev[] =
{
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const char *month_names[] =
{
  "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
};

const char *weekday_names_abbrev[] =
{
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char *weekday_names[] =
{
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};



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
gboolean faking_time;

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

/**
 * get_local_timezone_ofs:
 * @when: time in UTC
 *
 * Return the zone offset (measured in seconds) of @when expressed in local
 * time. The function also takes care about daylight saving.
 **/
long
get_local_timezone_ofs(time_t when)
{
#ifdef SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF
  struct tm ltm;

  cached_localtime(&when, &ltm);
  return ltm.tm_gmtoff;

#else

  struct tm gtm;
  struct tm ltm;
  long tzoff;

  cached_localtime(&when, &ltm);
  cached_gmtime(&when, &gtm);

  tzoff = (ltm.tm_hour - gtm.tm_hour) * 3600;
  tzoff += (ltm.tm_min - gtm.tm_min) * 60;
  tzoff += ltm.tm_sec - gtm.tm_sec;

  if (tzoff > 0 && (ltm.tm_year < gtm.tm_year || ltm.tm_mon < gtm.tm_mon || ltm.tm_mday < gtm.tm_mday))
    tzoff -= 86400;
  else if (tzoff < 0 && (ltm.tm_year > gtm.tm_year || ltm.tm_mon > gtm.tm_mon || ltm.tm_mday > gtm.tm_mday))
    tzoff += 86400;

  return tzoff;
#endif /* SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF */
}


void
clean_time_cache(void)
{
  memset(&gm_time_cache, 0, sizeof(gm_time_cache));
  memset(&local_time_cache, 0, sizeof(local_time_cache));
}

int
format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return g_snprintf(buf, buflen, "%c%02ld:%02ld",
                    gmtoff < 0 ? '-' : '+',
                    (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                    ((gmtoff < 0 ? -gmtoff : gmtoff) % 3600) / 60);
}

/**
 * check_nanosleep:
 *
 * Check if nanosleep() is accurate enough for sub-millisecond sleeping. If
 * it is not good enough, we're skipping the minor sleeps in LogReader to
 * balance the cost of returning to the main loop (e.g.  we're always going
 * to return to the main loop, instead of trying to wait for the writer).
 **/
gboolean
check_nanosleep(void)
{
  struct timespec start, stop, sleep_amount;
  glong diff;
  gint attempts;

  for (attempts = 0; attempts < 3; attempts++)
    {
      clock_gettime(CLOCK_MONOTONIC, &start);
      sleep_amount.tv_sec = 0;
      /* 0.1 msec */
      sleep_amount.tv_nsec = 1e5;

      while (nanosleep(&sleep_amount, &sleep_amount) < 0)
        ;

      clock_gettime(CLOCK_MONOTONIC, &stop);
      diff = timespec_diff_nsec(&stop, &start);
      if (diff < 5e5)
        return TRUE;
    }
  return FALSE;
}

/**
 * g_time_val_diff:
 * @t1: time value t1
 * @t2: time value t2
 *
 * Calculates the time difference between t1 and t2 in microseconds.
 * The result is positive if t1 is later than t2.
 *
 * Returns:
 * Time difference in microseconds
 */
glong
g_time_val_diff(GTimeVal *t1, GTimeVal *t2)
{
  g_assert(t1);
  g_assert(t2);
  return (t1->tv_sec - t2->tv_sec) * G_USEC_PER_SEC + (t1->tv_usec - t2->tv_usec);
}

void
timespec_add_msec(struct timespec *ts, glong msec)
{
  ts->tv_sec += msec / 1000;
  msec = msec % 1000;
  ts->tv_nsec += (glong) (msec * 1e6);
  if (ts->tv_nsec > 1e9)
    {
      ts->tv_nsec -= (glong) 1e9;
      ts->tv_sec++;
    }
}

glong
timespec_diff_msec(const struct timespec *t1, const struct timespec *t2)
{
  return (t1->tv_sec - t2->tv_sec) * 1e3 + (t1->tv_nsec - t2->tv_nsec) / 1e6;
}

glong
timespec_diff_nsec(struct timespec *t1, struct timespec *t2)
{
  return (glong)((t1->tv_sec - t2->tv_sec) * 1e9) + (t1->tv_nsec - t2->tv_nsec);
}

/* Determine (guess) the year for the month.
 *
 * It can be used for BSD logs, where year is missing.
 */
gint
determine_year_for_month(gint month, const struct tm *now)
{
  if (month == 11 && now->tm_mon == 0)
    return now->tm_year - 1;
  else if (month == 0 && now->tm_mon == 11)
    return now->tm_year + 1;
  else
    return now->tm_year;
}
