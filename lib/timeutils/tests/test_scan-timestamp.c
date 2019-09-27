/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Balázs Scheidler
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
#include "apphook.h"
#include "timeutils/scan-timestamp.h"
#include "timeutils/cache.h"
#include "timeutils/format.h"
#include "timeutils/conv.h"

#include <criterion/criterion.h>
#include "stopwatch.h"

static void
fake_time(time_t now)
{
  GTimeVal tv = { now, 123 * 1e3 };

  set_cached_time(&tv);
}

static void
fake_time_add(time_t diff)
{
  GTimeVal tv;

  cached_g_current_time(&tv);
  tv.tv_sec += diff;
  set_cached_time(&tv);
}

static gboolean
_parse_rfc3164(const gchar *ts, gchar isotimestamp[32])
{
  UnixTime stamp;
  const guchar *data = (const guchar *) ts;
  gint length = strlen(ts);
  GString *result = g_string_new("");
  WallClockTime wct = WALL_CLOCK_TIME_INIT;


  gboolean success = scan_rfc3164_timestamp(&data, &length, &wct);

  unix_time_unset(&stamp);
  convert_wall_clock_time_to_unix_time(&wct, &stamp);

  append_format_unix_time(&stamp, result, TS_FMT_ISO, stamp.ut_gmtoff, 3);
  strncpy(isotimestamp, result->str, 32);
  g_string_free(result, TRUE);
  return success;
}

static gboolean
_parse_rfc5424(const gchar *ts, gchar isotimestamp[32])
{
  UnixTime stamp;
  const guchar *data = (const guchar *) ts;
  gint length = strlen(ts);
  GString *result = g_string_new("");
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  gboolean success = scan_rfc5424_timestamp(&data, &length, &wct);

  unix_time_unset(&stamp);
  convert_wall_clock_time_to_unix_time(&wct, &stamp);

  append_format_unix_time(&stamp, result, TS_FMT_ISO, stamp.ut_gmtoff, 3);
  strncpy(isotimestamp, result->str, 32);
  g_string_free(result, TRUE);
  return success;
}

static gboolean
_rfc3164_timestamp_eq(const gchar *ts, const gchar *expected, gchar converted[32])
{
  cr_assert(_parse_rfc3164(ts, converted));
  return strcmp(converted, expected) == 0;
}

static gboolean
_rfc5424_timestamp_eq(const gchar *ts, const gchar *expected, gchar converted[32])
{
  cr_assert(_parse_rfc5424(ts, converted));
  return strcmp(converted, expected) == 0;
}

#define _expect_rfc3164_timestamp_eq(ts, expected) \
  ({ \
    gchar converted[32]; \
    cr_expect(_rfc3164_timestamp_eq(ts, expected, converted), "Parsed RFC3164 timestamp does not equal expected, ts=%s, converted=%s, expected=%s", ts, converted, expected); \
  })

#define _expect_rfc5424_timestamp_eq(ts, expected) \
  ({ \
    gchar converted[32]; \
    cr_expect(_rfc5424_timestamp_eq(ts, expected, converted), "Parsed RFC5424 timestamp does not equal expected, ts=%s, converted=%s, expected=%s", ts, converted, expected); \
  })

Test(parse_timestamp, standard_bsd_format)
{
  _expect_rfc3164_timestamp_eq("Oct  1 17:46:12", "2017-10-01T17:46:12.000+02:00");
}

Test(parse_timestamp, bsd_extensions)
{
  /* fractions of a second */
  _expect_rfc3164_timestamp_eq("Dec  3 09:10:12.987", "2017-12-03T09:10:12.987+01:00");

  /* year added at the end (linksys) */
  _expect_rfc3164_timestamp_eq("Dec  3 09:10:12 2019 ", "2019-12-03T09:10:12.000+01:00");

  /* year after the mon/day (cisco) */
  _expect_rfc3164_timestamp_eq("Dec  3 2019 09:10:12:", "2019-12-03T09:10:12.000+01:00");
  _expect_rfc3164_timestamp_eq("Dec  3 2019 09:10:12 ", "2019-12-03T09:10:12.000+01:00");
}

Test(parse_timestamp, standard_bsd_format_year_in_the_future)
{
  /* compared to 2017-12-13, this timestamp is from the future, so in the year 2018 */
  _expect_rfc3164_timestamp_eq("Jan  3 17:46:12.000", "2018-01-03T17:46:12.000+01:00");
}

Test(parse_timestamp, standard_bsd_format_year_in_the_past)
{
  /* Jan 03 09:32:21 CET 2018 */
  fake_time(1514968341);

  /* compared to 2018-03-03, this timestamp is from the past, so in the year 2017 */
  _expect_rfc3164_timestamp_eq("Dec 31 17:46:12", "2017-12-31T17:46:12.000+01:00");
}


Test(parse_timestamp, daylight_saving_behavior_at_spring_with_explicit_timezones)
{
  /* Mar 25 01:59:59 CET 2018 */
  fake_time(1521939599);

  for (gint i = 0; i < 3; i++)
    {
      _expect_rfc3164_timestamp_eq("2018-03-25T01:59:59+01:00", "2018-03-25T01:59:59.000+01:00");

      /* this is an invalid timestamp, in CEST we don't have any valid hours
       * between 02:00:00 and 02:59:59 AM.
       *
       * We need to ensure that the incoming timezone offset stays intact and
       * the hour remains.
       */

      _expect_rfc3164_timestamp_eq("2018-03-25T02:00:00+01:00", "2018-03-25T02:00:00.000+01:00");
      _expect_rfc3164_timestamp_eq("2018-03-25T02:00:00+02:00", "2018-03-25T02:00:00.000+02:00");
      _expect_rfc3164_timestamp_eq("2018-03-25T02:59:59+01:00", "2018-03-25T02:59:59.000+01:00");
      _expect_rfc3164_timestamp_eq("2018-03-25T02:59:59+02:00", "2018-03-25T02:59:59.000+02:00");

      /* 03:00:00 AM is already valid and DST is enabled */
      _expect_rfc3164_timestamp_eq("2018-03-25T03:00:00+02:00", "2018-03-25T03:00:00.000+02:00");
      fake_time_add(3600);
    }
}

Test(parse_timestamp, daylight_saving_behavior_at_spring_without_timezones)
{
  /* Mar 25 01:59:59 CET 2018 */
  fake_time(1521939599);

  /* iterate _before_, _within_, _after_ the transition hour */
  for (gint i = 0; i < 3; i++)
    {
      _expect_rfc3164_timestamp_eq("Mar 25 01:59:59", "2018-03-25T01:59:59.000+01:00");

      /* this is an invalid timestamp, in CEST we don't have any valid hours
       * between 02:00:00 and 02:59:59 AM.
       *
       * This behavior actually moves these timestamp 1 hour early, as the
       * "hour" value remains intact, but the timezone changes.
       *
       * Devices with timezone support should not be emitting these timestamps
       * at all, otherwise if we don't send timezone information, the hour
       * remains as we've received.  We could adjust the hour to 03:00, but in
       * that case, when the device actually reaches 03:00 we would go back in
       * time.  (e.g.  02:59:59 automatically changed by syslog-ng to 03:59:59,
       * then we would go back to 03:00:00 as that's already a valid timestamp.
       *
       */

      _expect_rfc3164_timestamp_eq("Mar 25 02:00:00", "2018-03-25T02:00:00.000+02:00");
      _expect_rfc3164_timestamp_eq("Mar 25 02:59:59", "2018-03-25T02:59:59.000+02:00");

      /* 03:00:00 AM is already valid and DST is enabled */
      _expect_rfc3164_timestamp_eq("Mar 25 03:00:00", "2018-03-25T03:00:00.000+02:00");
      fake_time_add(3600);
    }
}

Test(parse_timestamp, daylight_saving_detection_at_autumn_with_timezones)
{
  /* Oct 28 01:59:59 CEST 2018 */
  fake_time(1540684799);

  /* iterate _before_, _within_, _after_ the transition hour */
  for (gint i = 0; i < 3; i++)
    {
      _expect_rfc3164_timestamp_eq("2018-10-28T01:59:59.000+01:00", "2018-10-28T01:59:59.000+01:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T01:59:59.000+02:00", "2018-10-28T01:59:59.000+02:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T02:00:00.000+01:00", "2018-10-28T02:00:00.000+01:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T02:00:00.000+02:00", "2018-10-28T02:00:00.000+02:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T02:59:59.000+01:00", "2018-10-28T02:59:59.000+01:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T02:59:59.000+02:00", "2018-10-28T02:59:59.000+02:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T03:00:00.000+01:00", "2018-10-28T03:00:00.000+01:00");
      _expect_rfc3164_timestamp_eq("2018-10-28T03:00:00.000+02:00", "2018-10-28T03:00:00.000+02:00");

      fake_time_add(3600);
    }
}

/* NOTE: Platform specific issues
 *
 * At least macOS seems to do something funny thing when trying
 * to find out whether DST is in effect in a timestamp within the
 * transition period.  (e.g. mktime() with tm_isdst set to -1).
 *
 * On macOS For some reason, 02:00:00 - 02:25:17 is considered DST enabled,
 * while 02:25:18-02:59:59 are considered DST disabled.  The entire hour
 * is considered DST enabled on Linux.
 *
 * For this reason, I've dropped 02:59:59 testcases where there's no
 * timezone information, and added this note.
 */
Test(parse_timestamp, daylight_saving_detection_at_autumn_without_timezones)
{
  /* Oct 28 01:59:59 CEST 2018 */
  fake_time(1540684799);

  /* iterate _before_, _within_, _after_ the transition hour */
  for (gint i = 0; i < 3; i++)
    {
      _expect_rfc3164_timestamp_eq("Oct 28 01:59:59", "2018-10-28T01:59:59.000+02:00");
      _expect_rfc3164_timestamp_eq("Oct 28 02:00:00", "2018-10-28T02:00:00.000+02:00");
      _expect_rfc3164_timestamp_eq("Oct 28 03:00:00", "2018-10-28T03:00:00.000+01:00");

      fake_time_add(3600);
    }
}

Test(parse_timestamp, cisco_timestamps)
{
  /* BSD with fractions of a second */
  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40", "2017-04-29T13:58:40.000+02:00");
  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40:", "2017-04-29T13:58:40.000+02:00");
  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40.411", "2017-04-29T13:58:40.411+02:00");
  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40.411:", "2017-04-29T13:58:40.411+02:00");

  /* MMM DD YYYY HH:MM:SS */
  _expect_rfc3164_timestamp_eq("Apr 29 2016 13:58:40 ", "2016-04-29T13:58:40.000+02:00");
  _expect_rfc3164_timestamp_eq("Apr 29 2016 13:58:40:", "2016-04-29T13:58:40.000+02:00");

  /* NOTE: fractions of a second are not supported now
   *  _expect_rfc3164_timestamp_eq("Apr 29 2016 13:58:40.411 ", "2016-04-29T13:58:40.411+02:00");
   *  _expect_rfc3164_timestamp_eq("Apr 29 2016 13:58:40.411:", "2016-04-29T13:58:40.411+02:00");
   */

  /* LinkSys: MMM DD HH:MM:SS YYYY (maybe we should accept ":" as a terminator character? */
  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40 2016 ", "2016-04-29T13:58:40.000+02:00");

  /* NOTE: fractions of a second causes the year to be ignored.
   *  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40.411 2016 ", "2016-04-29T13:58:40.411+02:00");
   *  _expect_rfc3164_timestamp_eq("Apr 29 13:58:40.411 2016:", "2016-04-29T13:58:40.411+02:00");
   */

  /* Additional things we should cover in timestamp parsing (and tests)
   *  - msecs
   *  - timezone
   *  - AM/PM markers
   *  - year at various locations
   *
   * Cisco call manager produces this:
   *   _expect_rfc3164_timestamp_eq("Jun 14 11:57:27 PM.685 UTC:", "2017-06-14T23:57:27.685+00:00");
   */

}

Test(parse_timestamp, rfc5424_timestamps)
{
  _expect_rfc5424_timestamp_eq("2017-06-14T23:57:27+02:00", "2017-06-14T23:57:27.000+02:00");
  _expect_rfc5424_timestamp_eq("2017-06-14T23:57:27Z", "2017-06-14T23:57:27.000+00:00");
}

Test(parse_timestamp, rfc3164_performance)
{
  const gchar *ts = "Dec 14 05:27:22";
  const guchar *data = (const guchar *) ts;
  gint length = strlen(ts);
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gint it = 1000000;

  start_stopwatch();
  for (gint i = 0; i < it; i++)
    {
      scan_rfc3164_timestamp(&data, &length, &wct);
    }
  stop_stopwatch_and_display_result(it, "RFC3164 timestamp parsing speed");
}

Test(parse_timestamp, rfc5424_performance)
{
  const gchar *ts = "2019-12-14T05:27:22";
  const guchar *data = (const guchar *) ts;
  gint length = strlen(ts);
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gint it = 1000000;

  start_stopwatch();
  for (gint i = 0; i < it; i++)
    {
      scan_rfc5424_timestamp(&data, &length, &wct);
    }
  stop_stopwatch_and_display_result(it, "RFC5424 timestamp parsing speed");
}


void
setup(void)
{
  app_startup();
  setenv("TZ", "CET", TRUE);
  tzset();

  /* Dec 13 09:10:23 CET 2017 */
  fake_time(1513152623);
}

void
teardown(void)
{
  app_shutdown();
}

TestSuite(parse_timestamp, .init = setup, .fini = teardown);
