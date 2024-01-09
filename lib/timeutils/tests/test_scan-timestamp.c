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
#include <criterion/criterion.h>
#include "libtest/stopwatch.h"
#include "libtest/fake-time.h"

#include "timeutils/scan-timestamp.h"
#include "timeutils/cache.h"
#include "timeutils/format.h"
#include "timeutils/conv.h"
#include "apphook.h"


static gboolean
_parse_rfc3164(const gchar *ts, gint len, gchar isotimestamp[32])
{
  UnixTime stamp;
  const guchar *tsu = (const guchar *) ts;
  gint tsu_len = len < 0 ? strlen(ts) : len;
  GString *result = g_string_new("");
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  const guchar *data = tsu;
  gint length = tsu_len;
  gboolean success = scan_rfc3164_timestamp(&data, &length, &wct);

  cr_assert(length >= 0);
  cr_assert(data == &tsu[tsu_len - length]);

  unix_time_unset(&stamp);
  convert_wall_clock_time_to_unix_time(&wct, &stamp);

  append_format_unix_time(&stamp, result, TS_FMT_ISO, stamp.ut_gmtoff, 3);
  strncpy(isotimestamp, result->str, 32);
  g_string_free(result, TRUE);
  return success;
}

static gboolean
_parse_rfc5424(const gchar *ts, gint len, gchar isotimestamp[32])
{
  UnixTime stamp;
  const guchar *tsu = (const guchar *) ts;
  gint tsu_len = len < 0 ? strlen(ts) : len;
  GString *result = g_string_new("");
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  const guchar *data = tsu;
  gint length = tsu_len;
  gboolean success = scan_rfc5424_timestamp(&data, &length, &wct);

  cr_assert(length >= 0);
  cr_assert(data == &tsu[tsu_len - length]);

  unix_time_unset(&stamp);
  convert_wall_clock_time_to_unix_time(&wct, &stamp);

  append_format_unix_time(&stamp, result, TS_FMT_ISO, stamp.ut_gmtoff, 3);
  strncpy(isotimestamp, result->str, 32);
  g_string_free(result, TRUE);
  return success;
}

static gboolean
_rfc3164_timestamp_eq(const gchar *ts, gint len, const gchar *expected, gchar converted[32])
{
  cr_assert(_parse_rfc3164(ts, len, converted));
  return strcmp(converted, expected) == 0;
}

static gboolean
_rfc5424_timestamp_eq(const gchar *ts, gint len, const gchar *expected, gchar converted[32])
{
  cr_assert(_parse_rfc5424(ts, len, converted));
  return strcmp(converted, expected) == 0;
}

#define _expect_rfc3164_timestamp_eq(ts, expected) \
  ({ \
    gchar converted[32]; \
    cr_expect(_rfc3164_timestamp_eq(ts, -1, expected, converted), "Parsed RFC3164 timestamp does not equal expected, ts=%s, converted=%s, expected=%s", ts, converted, expected); \
  })

#define _expect_rfc3164_timestamp_len_eq(ts, len, expected) \
  ({ \
    gchar converted[32]; \
    cr_expect(_rfc3164_timestamp_eq(ts, len, expected, converted), "Parsed RFC3164 timestamp does not equal expected, ts=%s, converted=%s, expected=%s", ts, converted, expected); \
  })

#define _expect_rfc3164_fails(ts, len) \
  ({  \
    WallClockTime wct = WALL_CLOCK_TIME_INIT; \
    const guchar *data = (guchar *) ts; \
    gint length = len < 0 ? strlen(ts) : len; \
    cr_assert_not(scan_rfc3164_timestamp(&data, &length, &wct)); \
  })

#define _expect_rfc5424_timestamp_eq(ts, expected) \
  ({ \
    gchar converted[32]; \
    cr_expect(_rfc5424_timestamp_eq(ts, -1, expected, converted), "Parsed RFC5424 timestamp does not equal expected, ts=%s, converted=%s, expected=%s", ts, converted, expected); \
  })

#define _expect_rfc5424_timestamp_len_eq(ts, len, expected) \
  ({ \
    gchar converted[32]; \
    cr_expect(_rfc5424_timestamp_eq(ts, len, expected, converted), "Parsed RFC5424 timestamp does not equal expected, ts=%s, converted=%s, expected=%s", ts, converted, expected); \
  })

#define _expect_rfc5424_fails(ts, len) \
  ({  \
    WallClockTime wct = WALL_CLOCK_TIME_INIT; \
    const guchar *data = (guchar *) ts; \
    gint length = len < 0 ? strlen(ts) : len; \
    cr_assert_not(scan_rfc5424_timestamp(&data, &length, &wct)); \
  })


Test(parse_timestamp, standard_bsd_format)
{
  _expect_rfc3164_timestamp_eq("Oct  1 17:46:12", "2017-10-01T17:46:12.000+02:00");
}

Test(parse_timestamp, bsd_extensions)
{
  /* fractions of a second */
  _expect_rfc3164_timestamp_eq("Dec  3 09:10:12.987", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc3164_timestamp_eq("Dec  3 09:10:12,987", "2017-12-03T09:10:12.987+01:00");

  /* year added at the end (linksys) */
  _expect_rfc3164_timestamp_eq("Dec  3 09:10:12 2019 ", "2019-12-03T09:10:12.000+01:00");

  /* year after the mon/day (cisco) */
  _expect_rfc3164_timestamp_eq("Dec  3 2019 09:10:12:", "2019-12-03T09:10:12.000+01:00");
  _expect_rfc3164_timestamp_eq("Dec  3 2019 09:10:12 ", "2019-12-03T09:10:12.000+01:00");
}

Test(parse_timestamp, bsd_invalid)
{
  /* single space between month and mday */
  _expect_rfc3164_timestamp_eq("Dec 3 09:10:12", "2017-12-03T09:10:12.000+01:00");
  _expect_rfc3164_timestamp_eq("Dec 3 09:10:12.987", "2017-12-03T09:10:12.987+01:00");
}

Test(parse_timestamp, accept_iso_timestamps_with_space)
{
  _expect_rfc3164_timestamp_eq("2017-12-03 09:10:12.987+01:00", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc3164_timestamp_eq("2017-12-03 09:10:12,987+01:00", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc3164_timestamp_eq("2017-12-03 09:10:12.987", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc3164_timestamp_eq("2017-12-03 09:10:12,987", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc3164_timestamp_eq("2017-12-03 09:10:12", "2017-12-03T09:10:12.000+01:00");
  _expect_rfc5424_timestamp_eq("2017-12-03 09:10:12.987+01:00", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc5424_timestamp_eq("2017-12-03 09:10:12,987+01:00", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc5424_timestamp_eq("2017-12-03 09:10:12.987", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc5424_timestamp_eq("2017-12-03 09:10:12,987", "2017-12-03T09:10:12.987+01:00");
  _expect_rfc5424_timestamp_eq("2017-12-03 09:10:12", "2017-12-03T09:10:12.000+01:00");
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

Test(parse_timestamp, non_zero_terminated_rfc3164_iso_input_is_handled_properly)
{
  gchar *ts = "2022-08-17T05:02:28.417Z whatever";
  gint ts_len = 24;

  _expect_rfc3164_timestamp_len_eq(ts, strlen(ts), "2022-08-17T05:02:28.417+00:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len + 5, "2022-08-17T05:02:28.417+00:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len, "2022-08-17T05:02:28.417+00:00");

  /* no "Z" parsed, timezone defaults to local, forced CET */
  _expect_rfc3164_timestamp_len_eq(ts, ts_len - 1, "2022-08-17T05:02:28.417+02:00");

  /* msec is partially parsed as we trim the string from the right */
  _expect_rfc3164_timestamp_len_eq(ts, ts_len - 2, "2022-08-17T05:02:28.410+02:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len - 3, "2022-08-17T05:02:28.400+02:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len - 4, "2022-08-17T05:02:28.000+02:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len - 5, "2022-08-17T05:02:28.000+02:00");

  for (gint i = 6; i < ts_len; i++)
    _expect_rfc3164_fails(ts, ts_len - i);

}

Test(parse_timestamp, non_zero_terminated_rfc3164_bsd_pix_or_asa_input_is_handled_properly)
{
  gchar *ts = "Aug 17 2022 05:02:28: whatever";
  gint ts_len = 21;

  _expect_rfc3164_timestamp_len_eq(ts, strlen(ts), "2022-08-17T05:02:28.000+02:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len + 5, "2022-08-17T05:02:28.000+02:00");
  _expect_rfc3164_timestamp_len_eq(ts, ts_len, "2022-08-17T05:02:28.000+02:00");

  /* no ":" at the end, that's a problem, unrecognized */
  _expect_rfc3164_fails(ts, ts_len - 1);

  for (gint i = 1; i < ts_len; i++)
    _expect_rfc3164_fails(ts, ts_len - i);
}

Test(parse_timestamp, non_zero_terminated_rfc5424_input_is_handled_properly)
{
  gchar *ts = "2022-08-17T05:02:28.417Z whatever";
  gint ts_len = 24;

  _expect_rfc5424_timestamp_len_eq(ts, strlen(ts), "2022-08-17T05:02:28.417+00:00");
  _expect_rfc5424_timestamp_len_eq(ts, ts_len + 5, "2022-08-17T05:02:28.417+00:00");
  _expect_rfc5424_timestamp_len_eq(ts, ts_len, "2022-08-17T05:02:28.417+00:00");

  /* no "Z" parsed, timezone defaults to local, forced CET */
  _expect_rfc5424_timestamp_len_eq(ts, ts_len - 1, "2022-08-17T05:02:28.417+02:00");

  /* msec is partially parsed as we trim the string from the right */
  _expect_rfc5424_timestamp_len_eq(ts, ts_len - 2, "2022-08-17T05:02:28.410+02:00");
  _expect_rfc5424_timestamp_len_eq(ts, ts_len - 3, "2022-08-17T05:02:28.400+02:00");
  _expect_rfc5424_timestamp_len_eq(ts, ts_len - 4, "2022-08-17T05:02:28.000+02:00");
  _expect_rfc5424_timestamp_len_eq(ts, ts_len - 5, "2022-08-17T05:02:28.000+02:00");

  for (gint i = 6; i < ts_len; i++)
    _expect_rfc5424_fails(ts, ts_len - i);

}

Test(parse_timestamp, non_zero_terminated_rfc5424_timestamp_only)
{
  const gchar *ts = "2022-08-17T05:02:28.417+03:00";
  gint ts_len = strlen(ts);
  _expect_rfc5424_timestamp_len_eq(ts, ts_len, ts);
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

static void
_parse_valid_month(const gchar *month, const gint expected_month)
{
  gint left = strlen(month);
  gint mon = -1;

  cr_assert(scan_month_abbrev(&month, &left, &mon));

  cr_assert_eq(mon, expected_month);
  cr_assert_eq(left, 0);
}

Test(scan_month_abbrev, valid_months)
{
  _parse_valid_month("Jan", 0);
  _parse_valid_month("Feb", 1);
  _parse_valid_month("Mar", 2);
  _parse_valid_month("Apr", 3);
  _parse_valid_month("May", 4);
  _parse_valid_month("Jun", 5);
  _parse_valid_month("Jul", 6);
  _parse_valid_month("Aug", 7);
  _parse_valid_month("Sep", 8);
  _parse_valid_month("Oct", 9);
  _parse_valid_month("Nov", 10);
  _parse_valid_month("Dec", 11);
}

static void
_parse_invalid_month(const gchar *month)
{
  gint left = strlen(month);
  gint original_left = left;
  gint mon = -1;

  cr_assert_not(scan_month_abbrev(&month, &left, &mon));

  cr_assert_eq(mon, -1);
  cr_assert_eq(left, original_left);
}

Test(scan_month_abbrev, invalid_month_names)
{
  _parse_invalid_month("");
  _parse_invalid_month("Set");
  _parse_invalid_month("Jit");
  _parse_invalid_month("abcdefg");

  _parse_invalid_month("JaX");
  _parse_invalid_month("FeX");
  _parse_invalid_month("MaX");
  _parse_invalid_month("ApX");
  _parse_invalid_month("MaX");
  _parse_invalid_month("JuX");
  _parse_invalid_month("JuX");
  _parse_invalid_month("AuX");
  _parse_invalid_month("SeX");
  _parse_invalid_month("OcX");
  _parse_invalid_month("NoX");
  _parse_invalid_month("DeX");
}

static void
_parse_valid_day(const gchar *day, const gint expected_day)
{
  gint left = strlen(day);
  gint daynum = -1;

  cr_assert(scan_day_abbrev(&day, &left, &daynum));

  cr_assert_eq(daynum, expected_day);
  cr_assert_eq(left, 0);
}

Test(scan_day_abbrev, valid_days)
{
  _parse_valid_day("Sun", 0);
  _parse_valid_day("Mon", 1);
  _parse_valid_day("Tue", 2);
  _parse_valid_day("Wed", 3);
  _parse_valid_day("Thu", 4);
  _parse_valid_day("Fri", 5);
  _parse_valid_day("Sat", 6);
}

static void
_parse_invalid_day(const gchar *day)
{
  gint left = strlen(day);
  gint original_left = left;
  gint daynum = -1;

  cr_assert_not(scan_day_abbrev(&day, &left, &daynum));

  cr_assert_eq(daynum, -1);
  cr_assert_eq(left, original_left);
}

Test(scan_day_abbrev, invalid_day_names)
{
  _parse_invalid_day("");
  _parse_invalid_day("Set");
  _parse_invalid_day("abcdefg");

  _parse_invalid_day("SuX");
  _parse_invalid_day("MoX");
  _parse_invalid_day("TuX");
  _parse_invalid_day("WeX");
  _parse_invalid_day("ThX");
  _parse_invalid_day("FrX");
  _parse_invalid_day("SaX");
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
