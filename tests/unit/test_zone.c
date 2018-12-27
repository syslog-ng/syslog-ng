/*
 * Copyright (c) 2005-2015 Balabit
 * Copyright (c) 2005-2011 Balázs Scheidler
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

#include "apphook.h"
#include "timeutils/timeutils.h"
#include "timeutils/timeutils.h"
#include "logstamp.h"
#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct _TimezoneOffsetTestCase
{
  const gchar *time_zone;
  time_t utc;
  long expected_offset;
} TimezoneOffsetTestCase;

typedef struct _TimezoneTestCase
{
  const time_t stamp_to_test;
  const gchar *time_zone;
} TimezoneTestCase;

typedef struct _TimestampFormatTestCase
{
  gint format;
  glong zone_offset;
  gint frac_digits;
  gchar *expected_format;
} TimestampFormatTestCase;

void
set_time_zone(const gchar *time_zone)
{
  setenv("TZ", time_zone, TRUE);
  tzset();
  clean_time_cache();
}

int
time_zone_exists(const char *time_zone)
{
  TimeZoneInfo *info;

  set_time_zone(time_zone);
  info = time_zone_info_new(time_zone);

  if (info)
    {
      time_zone_info_free(info);
      return TRUE;
    }
  return FALSE;
}

void
test_time_zone(const time_t stamp_to_test, const char *time_zone)
{
  TimeZoneInfo *info;
  time_t offset, expected_offset;

  set_time_zone(time_zone);
  info = time_zone_info_new(time_zone);
  offset = time_zone_info_get_offset(info, stamp_to_test);
  expected_offset = get_local_timezone_ofs(stamp_to_test);
  cr_assert_eq(difftime(offset, expected_offset), 0,
               "unixtimestamp: %ld TimeZoneName (%s) localtime offset(%ld), timezone file offset(%ld)\n",
               (glong) stamp_to_test, time_zone, (glong) expected_offset, (glong) offset);

  time_zone_info_free(info);
}

void
assert_time_zone(TimezoneTestCase c)
{
  if (!time_zone_exists(c.time_zone))
    {
      printf("SKIP: %s\n", c.time_zone);
      return;
    }
  test_time_zone(c.stamp_to_test, c.time_zone);
  test_time_zone(c.stamp_to_test - 6*30*24*60*60, c.time_zone);
  test_time_zone(c.stamp_to_test + 6*30*24*6, c.time_zone);
}

void
assert_time_zone_offset(TimezoneOffsetTestCase c)
{
  long offset;

  set_time_zone(c.time_zone);

  offset = get_local_timezone_ofs(c.utc);
  cr_assert_eq(offset, c.expected_offset,
               "Timezone offset mismatch: zone: %s, %ld, expected %ld\n", c.time_zone, offset, c.expected_offset);
}

void
assert_timestamp_format(GString *target, LogStamp *stamp, TimestampFormatTestCase c)
{
  log_stamp_format(stamp, target, c.format, c.zone_offset, c.frac_digits);
  cr_assert_str_eq(target->str, c.expected_format, "Actual: %s, Expected: %s", target->str, c.expected_format);
}

TestSuite(zone, .init = app_startup, .fini = app_shutdown);

Test(zone, test_time_zone_offset)
{
  TimezoneOffsetTestCase test_cases[] =
  {
    /* 2005-10-14 21:47:37 CEST, dst enabled */
    {"MET-1METDST", 1129319257, 7200},
    /* 2005-11-14 10:10:00 CET, dst disabled */
    {"MET-1METDST", 1131959400, 3600},
    /* 2005-10-14 21:47:37 GMT, no DST */
    {"GMT", 1129319257, 0},
    /* 2005-11-14 10:10:00 GMT, no DST */
    {"GMT", 1131959400, 0},
    /* 2005-04-03 01:30:00 ESD, DST disabled */
    {"EST5EDT", 1112509800, -5*3600},
    /* 2005-04-03 01:59:59 ESD, DST disabled */
    {"EST5EDT", 1112511599, -5*3600},
    /* 2005-04-03 03:00:00 EDT, DST enabled */
    {"EST5EDT", 1112511600, -4*3600},
    /* 2005-04-03 03:00:01 EDT, DST enabled */
    {"EST5EDT", 1112511601, -4*3600},
    /* 2005-10-30 01:59:59 EDT, DST enabled */
    {"EST5EDT", 1130651999, -4*3600},
    /* 2005-10-30 01:00:00 EST, DST disabled */
    {"EST5EDT", 1130652000, -5*3600},
    /* 2007-03-11 01:00:00 EST, DST disabled */
    {"EST5EDT", 1173592800, -5*3600},
    /* 2007-03-11 01:59:59 EST, DST disabled */
    {"EST5EDT", 1173596399, -5*3600},
    /* 2007-03-11 03:00:00 EST, DST enabled */
    {"EST5EDT", 1173596400, -4*3600},
    /* 2007-11-04 01:59:59 EST, DST enabled */
    {"EST5EDT", 1194155999, -4*3600},
    /* 2007-11-04 01:00:00 EST, DST disabled */
    {"EST5EDT", 1194156000, -5*3600},
    /* Oct 31 01:59:59 2004 (EST) +1000 */
    {"Australia/Victoria", 1099151999, 10*3600},
    /* Oct 31 03:00:00 2004 (EST) +1100 */
    {"Australia/Victoria", 1099152000, 11*3600},
    /* Mar 27 02:59:59 2005 (EST) +1100 */
    {"Australia/Victoria", 1111852799, 11*3600},
    /* Mar 27 02:00:00 2005 (EST) +1000 */
    {"Australia/Victoria", 1111852800, 10*3600},
    /* Oct  2 01:59:59 2004 (NZT) +1200 */
    {"NZ", 1128175199, 12*3600},
    /* Oct  2 03:00:00 2004 (NZDT) +1300 */
    {"NZ", 1128175200, 13*3600},
    /* Mar 20 02:59:59 2005 (NZDT) +1300 */
    {"NZ", 1111240799, 13*3600},
    /* Mar 20 02:00:00 2005 (NZT) +1200 */
    {"NZ", 1111240800, 12*3600},
  };
  gint i, nr_of_cases;

  nr_of_cases = sizeof(test_cases) / sizeof(test_cases[0]);
  for (i = 0; i < nr_of_cases; i++)
    assert_time_zone_offset(test_cases[i]);

}

Test(zone, test_time_zones)
{
  time_t now = time(NULL);

  TimezoneTestCase test_cases[] =
  {
    {now, "America/Louisville"},
    {now, "Hongkong"},
    {now, "Europe/Budapest"},
    {1288486800, "Europe/Budapest"},
    {1288486860, "Europe/Budapest"},
    {1288486740, "Europe/Budapest"},
    {1288486800-1800, "Europe/Budapest"},
    {1288486800+1800, "Europe/Budapest"},
    {1288486800-3600, "Europe/Budapest"},
    {1288486800+3600, "Europe/Budapest"},
    {1288486800-3601, "Europe/Budapest"},
    {1288486800+3601, "Europe/Budapest"},
    {now, "Asia/Kuala_Lumpur"},
    {now, "CST6CDT"},
    {now, "US/Pacific"},
    {now, "US/Indiana-Starke"},
    {now, "US/Samoa"},
    {now, "US/Arizona"},
    {now, "US/Aleutian"},
    {now, "US/Michigan"},
    {now, "US/Alaska"},
    {now, "US/Central"},
    {now, "US/Hawaii"},
    {now, "US/East-Indiana"},
    {now, "US/Eastern"},
    {now, "US/Mountain"},
    {now, "Pacific/Noumea"},
    {now, "Pacific/Samoa"},
    {now, "Pacific/Apia"},
    {now, "Pacific/Auckland"},
    {now, "Pacific/Fakaofo"},
    {now, "Pacific/Saipan"},
    {now, "Pacific/Easter"},
    {now, "Pacific/Norfolk"},
    {now, "Pacific/Kosrae"},
    {now, "Pacific/Tarawa"},
    {now, "Pacific/Tahiti"},
    {now, "Pacific/Pago_Pago"},
    {now, "Pacific/Majuro"},
    {now, "Pacific/Guam"},
    {now, "Pacific/Ponape"},
    {now, "Pacific/Tongatapu"},
    {now, "Pacific/Funafuti"},
    {now, "Pacific/Wallis"},
    {now, "Pacific/Efate"},
    {now, "Pacific/Marquesas"},
    {now, "Pacific/Enderbury"},
    {now, "Pacific/Pitcairn"},
    {now, "Pacific/Yap"},
    {now, "Pacific/Wake"},
    {now, "Pacific/Johnston"},
    {now, "Pacific/Kwajalein"},
    {now, "Pacific/Chatham"},
    {now, "Pacific/Gambier"},
    {now, "Pacific/Galapagos"},
    {now, "Pacific/Kiritimati"},
    {now, "Pacific/Honolulu"},
    {now, "Pacific/Truk"},
    {now, "Pacific/Midway"},
    {now, "Pacific/Fiji"},
    {now, "Pacific/Rarotonga"},
    {now, "Pacific/Guadalcanal"},
    {now, "Pacific/Nauru"},
    {now, "Pacific/Palau"},
    {now, "Pacific/Port_Moresby"},
    {now, "Pacific/Niue"},
    {now, "GMT"},
    {now, "Hongkong"},
    {now, "ROK"},
    {now, "Navajo"},
    {now, "ROC"},
    {now, "WET"},
    {now, "posixrules"},
    {now, "CET"},
    {now, "MET"},
    {now, "MST"},
    {now, "Turkey"},
    {now, "Zulu"},
    {now, "GMT+0"},
    {now, "Canada/Saskatchewan"},
    {now, "Canada/Pacific"},
    {now, "Canada/Yukon"},
    {now, "Canada/East-Saskatchewan"},
    {now, "Canada/Newfoundland"},
    {now, "Canada/Central"},
    {now, "Canada/Eastern"},
    {now, "Canada/Atlantic"},
    {now, "Canada/Mountain"},
    {now, "Singapore"},
    {now, "UCT"},
    {now, "Poland"},
    {now, "Chile/Continental"},
    {now, "Chile/EasterIsland"},
    {now, "Portugal"},
    {now, "Egypt"},
    {now, "Japan"},
    {now, "Iran"},
    {now, "EET"},
    {now, "Europe/Oslo"},
    {now, "Europe/Bratislava"},
    {now, "Europe/Gibraltar"},
    {now, "Europe/Skopje"},
    {now, "Europe/Simferopol"},
    {now, "Europe/Zurich"},
    {now, "Europe/Vienna"},
    {now, "Europe/Paris"},
    {now, "Europe/Jersey"},
    {now, "Europe/Tallinn"},
    {now, "Europe/Madrid"},
    {now, "Europe/Volgograd"},
    {now, "Europe/Zaporozhye"},
    {now, "Europe/Mariehamn"},
    {now, "Europe/Vaduz"},
    {now, "Europe/Moscow"},
    {now, "Europe/Stockholm"},
    {now, "Europe/Minsk"},
    {now, "Europe/Andorra"},
    {now, "Europe/Istanbul"},
    {now, "Europe/Tiraspol"},
    {now, "Europe/Podgorica"},
    {now, "Europe/Bucharest"},
    {now, "Europe/Ljubljana"},
    {now, "Europe/Brussels"},
    {now, "Europe/Amsterdam"},
    {now, "Europe/Riga"},
    {now, "Europe/Tirane"},
    {now, "Europe/Berlin"},
    {now, "Europe/Guernsey"},
    {now, "Europe/Warsaw"},
    {now, "Europe/Vatican"},
    {now, "Europe/Malta"},
    {now, "Europe/Nicosia"},
    {now, "Europe/Budapest"},
    {now, "Europe/Kaliningrad"},
    {now, "Europe/Sarajevo"},
    {now, "Europe/Isle_of_Man"},
    {now, "Europe/Rome"},
    {now, "Europe/London"},
    {now, "Europe/Vilnius"},
    {now, "Europe/Samara"},
    {now, "Europe/Belfast"},
    {now, "Europe/Athens"},
    {now, "Europe/Copenhagen"},
    {now, "Europe/Belgrade"},
    {now, "Europe/Sofia"},
    {now, "Europe/San_Marino"},
    {now, "Europe/Helsinki"},
    {now, "Europe/Uzhgorod"},
    {now, "Europe/Monaco"},
    {now, "Europe/Prague"},
    {now, "Europe/Zagreb"},
    {now, "Europe/Lisbon"},
    {now, "Europe/Chisinau"},
    {now, "Europe/Dublin"},
    {now, "Europe/Luxembourg"},
    {now, "Europe/Kiev"},
    {now, "Jamaica"},
    {now, "Indian/Chagos"},
    {now, "Indian/Comoro"},
    {now, "Indian/Mauritius"},
    {now, "Indian/Mayotte"},
    {now, "Indian/Kerguelen"},
    {now, "Indian/Maldives"},
    {now, "Indian/Antananarivo"},
    {now, "Indian/Mahe"},
    {now, "Indian/Cocos"},
    {now, "Indian/Christmas"},
    {now, "Indian/Reunion"},
    {now, "Africa/Accra"},
    {now, "Africa/Lubumbashi"},
    {now, "Africa/Bangui"},
    {now, "Africa/Asmara"},
    {now, "Africa/Freetown"},
    {now, "Africa/Mbabane"},
    {now, "Africa/Djibouti"},
    {now, "Africa/Ndjamena"},
    {now, "Africa/Porto-Novo"},
    {now, "Africa/Nouakchott"},
    {now, "Africa/Brazzaville"},
    {now, "Africa/Tunis"},
    {now, "Africa/Dar_es_Salaam"},
    {now, "Africa/Lusaka"},
    {now, "Africa/Banjul"},
    {now, "Africa/Sao_Tome"},
    {now, "Africa/Monrovia"},
    {now, "Africa/Lagos"},
    {now, "Africa/Conakry"},
    {now, "Africa/Tripoli"},
    {now, "Africa/Algiers"},
    {now, "Africa/Casablanca"},
    {now, "Africa/Lome"},
    {now, "Africa/Bamako"},
    {now, "Africa/Nairobi"},
    {now, "Africa/Douala"},
    {now, "Africa/Addis_Ababa"},
    {now, "Africa/El_Aaiun"},
    {now, "Africa/Ceuta"},
    {now, "Africa/Harare"},
    {now, "Africa/Libreville"},
    {now, "Africa/Johannesburg"},
    {now, "Africa/Timbuktu"},
    {now, "Africa/Niamey"},
    {now, "Africa/Windhoek"},
    {now, "Africa/Bissau"},
    {now, "Africa/Maputo"},
    {now, "Africa/Kigali"},
    {now, "Africa/Dakar"},
    {now, "Africa/Ouagadougou"},
    {now, "Africa/Gaborone"},
    {now, "Africa/Khartoum"},
    {now, "Africa/Bujumbura"},
    {now, "Africa/Luanda"},
    {now, "Africa/Malabo"},
    {now, "Africa/Asmera"},
    {now, "Africa/Maseru"},
    {now, "Africa/Abidjan"},
    {now, "Africa/Kinshasa"},
    {now, "Africa/Blantyre"},
    {now, "Africa/Cairo"},
    {now, "Africa/Kampala"},
    {now, "Africa/Mogadishu"},
    {now, "Universal"},
    {now, "EST"},
    {now, "Greenwich"},
    {now, "Eire"},
    {now, "Asia/Qatar"},
    {now, "Asia/Makassar"},
    {now, "Asia/Colombo"},
    {now, "Asia/Chungking"},
    {now, "Asia/Macau"},
    {now, "Asia/Choibalsan"},
    {now, "Asia/Rangoon"},
    {now, "Asia/Harbin"},
    {now, "Asia/Yerevan"},
    {now, "Asia/Brunei"},
    {now, "Asia/Omsk"},
    {now, "Asia/Chongqing"},
    {now, "Asia/Tbilisi"},
    {now, "Asia/Tehran"},
    {now, "Asia/Manila"},
    {now, "Asia/Yakutsk"},
    {now, "Asia/Qyzylorda"},
    {now, "Asia/Dhaka"},
    {now, "Asia/Singapore"},
    {now, "Asia/Jakarta"},
    {now, "Asia/Novosibirsk"},
    {now, "Asia/Saigon"},
    {now, "Asia/Krasnoyarsk"},
    {now, "Asia/Kabul"},
    {now, "Asia/Bahrain"},
    {now, "Asia/Urumqi"},
    {now, "Asia/Thimbu"},
    {now, "Asia/Ulan_Bator"},
    {now, "Asia/Taipei"},
    {now, "Asia/Tashkent"},
    {now, "Asia/Dacca"},
    {now, "Asia/Aqtau"},
    {now, "Asia/Seoul"},
    {now, "Asia/Istanbul"},
    {now, "Asia/Tel_Aviv"},
    {now, "Asia/Almaty"},
    {now, "Asia/Phnom_Penh"},
    {now, "Asia/Baku"},
    {now, "Asia/Anadyr"},
    {now, "Asia/Aqtobe"},
    {now, "Asia/Kuwait"},
    {now, "Asia/Irkutsk"},
    {now, "Asia/Ulaanbaatar"},
    {now, "Asia/Tokyo"},
    {now, "Asia/Gaza"},
    {now, "Asia/Yekaterinburg"},
    {now, "Asia/Nicosia"},
    {now, "Asia/Jayapura"},
    {now, "Asia/Shanghai"},
    {now, "Asia/Pyongyang"},
    {now, "Asia/Macao"},
    {now, "Asia/Dushanbe"},
    {now, "Asia/Kuching"},
    {now, "Asia/Vientiane"},
    {now, "Asia/Riyadh"},
    {now, "Asia/Dili"},
    {now, "Asia/Samarkand"},
    {now, "Asia/Ashkhabad"},
    {now, "Asia/Calcutta"},
    {now, "Asia/Hong_Kong"},
    {now, "Asia/Sakhalin"},
    {now, "Asia/Beirut"},
    {now, "Asia/Damascus"},
    {now, "Asia/Katmandu"},
    {now, "Asia/Jerusalem"},
    {now, "Asia/Vladivostok"},
    {now, "Asia/Kamchatka"},
    {now, "Asia/Dubai"},
    {now, "Asia/Kashgar"},
    {now, "Asia/Ashgabat"},
    {now, "Asia/Amman"},
    {now, "Asia/Karachi"},
    {now, "Asia/Bangkok"},
    {now, "Asia/Oral"},
    {now, "Asia/Thimphu"},
    {now, "Asia/Bishkek"},
    {now, "Asia/Baghdad"},
    {now, "Asia/Kuala_Lumpur"},
    {now, "Asia/Pontianak"},
    {now, "Asia/Ujung_Pandang"},
    {now, "Asia/Muscat"},
    {now, "Asia/Aden"},
    {now, "Asia/Hovd"},
    {now, "Asia/Magadan"},
    {now, "EST5EDT"},
    {now, "PST8PDT"},
    {now, "Atlantic/Bermuda"},
    {now, "Atlantic/St_Helena"},
    {now, "Atlantic/Cape_Verde"},
    {now, "Atlantic/Stanley"},
    {now, "Atlantic/Azores"},
    {now, "Atlantic/Jan_Mayen"},
    {now, "Atlantic/Reykjavik"},
    {now, "Atlantic/Madeira"},
    {now, "Atlantic/Canary"},
    {now, "Atlantic/Faeroe"},
    {now, "Atlantic/Faroe"},
    {now, "Atlantic/South_Georgia"},
    {now, "Kwajalein"},
    {now, "UTC"},
    {now, "GMT-0"},
    {now, "MST7MDT"},
    {now, "GB-Eire"},
    {now, "PRC"},
    {now, "Arctic/Longyearbyen"},
    {now, "Cuba"},
    {now, "Israel"},
    {now, "Etc/GMT-3"},
    {now, "Etc/GMT+1"},
    {now, "Etc/GMT-5"},
    {now, "Etc/GMT"},
    {now, "Etc/GMT-13"},
    {now, "Etc/GMT-1"},
    {now, "Etc/GMT-9"},
    {now, "Etc/Zulu"},
    {now, "Etc/GMT+0"},
    {now, "Etc/UCT"},
    {now, "Etc/GMT+12"},
    {now, "Etc/GMT+9"},
    {now, "Etc/GMT-6"},
    {now, "Etc/Universal"},
    {now, "Etc/GMT-2"},
    {now, "Etc/Greenwich"},
    {now, "Etc/GMT+3"},
    {now, "Etc/GMT+8"},
    {now, "Etc/UTC"},
    {now, "Etc/GMT-0"},
    {now, "Etc/GMT-14"},
    {now, "Etc/GMT+10"},
    {now, "Etc/GMT+4"},
    {now, "Etc/GMT+5"},
    {now, "Etc/GMT-12"},
    {now, "Etc/GMT-8"},
    {now, "Etc/GMT+7"},
    {now, "Etc/GMT-11"},
    {now, "Etc/GMT+6"},
    {now, "Etc/GMT0"},
    {now, "Etc/GMT-7"},
    {now, "Etc/GMT+11"},
    {now, "Etc/GMT-4"},
    {now, "Etc/GMT+2"},
    {now, "Etc/GMT-10"},
    {now, "HST"},
    {now, "Iceland"},
    {now, "Mexico/BajaNorte"},
    {now, "Mexico/BajaSur"},
    {now, "Mexico/General"},
    {now, "Antarctica/Davis"},
    {now, "Antarctica/DumontDUrville"},
    {now, "Antarctica/Syowa"},
    {now, "Antarctica/Palmer"},
    {now, "Antarctica/Casey"},
    {now, "Antarctica/Rothera"},
    {now, "Antarctica/Mawson"},
    {now, "Antarctica/McMurdo"},
    {now, "Antarctica/South_Pole"},
    {now, "Antarctica/Vostok"},
    {now, "America/Curacao"},
    {now, "America/St_Lucia"},
    {now, "America/Managua"},
    {now, "America/Lima"},
    {now, "America/Nipigon"},
    {now, "America/Cayenne"},
    {now, "America/Detroit"},
    {now, "America/Kentucky/Louisville"},
    {now, "America/Kentucky/Monticello"},
    {now, "America/Belem"},
    {now, "America/Jujuy"},
    {now, "America/Godthab"},
    {now, "America/Guatemala"},
    {now, "America/Atka"},
    {now, "America/Montreal"},
    {now, "America/Resolute"},
    {now, "America/Thunder_Bay"},
    {now, "America/North_Dakota/New_Salem"},
    {now, "America/North_Dakota/Center"},
    {now, "America/Panama"},
    {now, "America/Los_Angeles"},
    {now, "America/Whitehorse"},
    {now, "America/Santiago"},
    {now, "America/Iqaluit"},
    {now, "America/Dawson"},
    {now, "America/Juneau"},
    {now, "America/Thule"},
    {now, "America/Fortaleza"},
    {now, "America/Montevideo"},
    {now, "America/Tegucigalpa"},
    {now, "America/Port-au-Prince"},
    {now, "America/Guadeloupe"},
    {now, "America/Coral_Harbour"},
    {now, "America/Monterrey"},
    {now, "America/Anguilla"},
    {now, "America/Antigua"},
    {now, "America/Campo_Grande"},
    {now, "America/Buenos_Aires"},
    {now, "America/Maceio"},
    {now, "America/New_York"},
    {now, "America/Puerto_Rico"},
    {now, "America/Noronha"},
    {now, "America/Sao_Paulo"},
    {now, "America/Cancun"},
    {now, "America/Aruba"},
    {now, "America/Yellowknife"},
    {now, "America/Knox_IN"},
    {now, "America/Halifax"},
    {now, "America/Grand_Turk"},
    {now, "America/Vancouver"},
    {now, "America/Bogota"},
    {now, "America/Santo_Domingo"},
    {now, "America/Tortola"},
    {now, "America/Blanc-Sablon"},
    {now, "America/St_Thomas"},
    {now, "America/Scoresbysund"},
    {now, "America/Jamaica"},
    {now, "America/El_Salvador"},
    {now, "America/Montserrat"},
    {now, "America/Martinique"},
    {now, "America/Nassau"},
    {now, "America/Indianapolis"},
    {now, "America/Pangnirtung"},
    {now, "America/Port_of_Spain"},
    {now, "America/Mexico_City"},
    {now, "America/Denver"},
    {now, "America/Dominica"},
    {now, "America/Eirunepe"},
    {now, "America/Atikokan"},
    {now, "America/Glace_Bay"},
    {now, "America/Rainy_River"},
    {now, "America/St_Barthelemy"},
    {now, "America/Miquelon"},
    {now, "America/Indiana/Vevay"},
    {now, "America/Indiana/Vincennes"},
    {now, "America/Indiana/Marengo"},
    {now, "America/Indiana/Petersburg"},
    {now, "America/Indiana/Tell_City"},
    {now, "America/Indiana/Knox"},
    {now, "America/Indiana/Indianapolis"},
    {now, "America/Indiana/Winamac"},
    {now, "America/Menominee"},
    {now, "America/Porto_Velho"},
    {now, "America/Phoenix"},
    {now, "America/Argentina/San_Juan"},
    {now, "America/Argentina/Jujuy"},
    {now, "America/Argentina/Ushuaia"},
    {now, "America/Argentina/Buenos_Aires"},
    {now, "America/Argentina/La_Rioja"},
    {now, "America/Argentina/ComodRivadavia"},
    {now, "America/Argentina/Tucuman"},
    {now, "America/Argentina/Rio_Gallegos"},
    {now, "America/Argentina/Mendoza"},
    {now, "America/Argentina/Cordoba"},
    {now, "America/Argentina/Catamarca"},
    {now, "America/Dawson_Creek"},
    {now, "America/Merida"},
    {now, "America/Moncton"},
    {now, "America/Goose_Bay"},
    {now, "America/Grenada"},
    {now, "America/Barbados"},
    {now, "America/Tijuana"},
    {now, "America/Regina"},
    {now, "America/Ensenada"},
    {now, "America/Louisville"},
    {now, "America/Edmonton"},
    {now, "America/Bahia"},
    {now, "America/Nome"},
    {now, "America/Guayaquil"},
    {now, "America/La_Paz"},
    {now, "America/Costa_Rica"},
    {now, "America/Mazatlan"},
    {now, "America/Havana"},
    {now, "America/Marigot"},
    {now, "America/Mendoza"},
    {now, "America/Virgin"},
    {now, "America/Manaus"},
    {now, "America/Rosario"},
    {now, "America/Boa_Vista"},
    {now, "America/Winnipeg"},
    {now, "America/Paramaribo"},
    {now, "America/Danmarkshavn"},
    {now, "America/Caracas"},
    {now, "America/Swift_Current"},
    {now, "America/St_Johns"},
    {now, "America/Araguaina"},
    {now, "America/Adak"},
    {now, "America/Cordoba"},
    {now, "America/Fort_Wayne"},
    {now, "America/Catamarca"},
    {now, "America/Recife"},
    {now, "America/Toronto"},
    {now, "America/Anchorage"},
    {now, "America/St_Vincent"},
    {now, "America/St_Kitts"},
    {now, "America/Chihuahua"},
    {now, "America/Cayman"},
    {now, "America/Belize"},
    {now, "America/Cambridge_Bay"},
    {now, "America/Cuiaba"},
    {now, "America/Chicago"},
    {now, "America/Guyana"},
    {now, "America/Inuvik"},
    {now, "America/Asuncion"},
    {now, "America/Porto_Acre"},
    {now, "America/Hermosillo"},
    {now, "America/Shiprock"},
    {now, "America/Rio_Branco"},
    {now, "America/Yakutat"},
    {now, "America/Rankin_Inlet"},
    {now, "America/Boise"},
    {now, "Brazil/West"},
    {now, "Brazil/Acre"},
    {now, "Brazil/East"},
    {now, "Brazil/DeNoronha"},
    {now, "GMT0"},
    {now, "Libya"},
    {now, "W-SU"},
    {now, "NZ-CHAT"},
    {now, "Factory"},
    {now, "GB"},
    {now, "Australia/West"},
    {now, "Australia/Canberra"},
    {now, "Australia/Broken_Hill"},
    {now, "Australia/Eucla"},
    {now, "Australia/Currie"},
    {now, "Australia/South"},
    {now, "Australia/Lindeman"},
    {now, "Australia/Hobart"},
    {now, "Australia/Perth"},
    {now, "Australia/Yancowinna"},
    {now, "Australia/Victoria"},
    {now, "Australia/Sydney"},
    {now, "Australia/North"},
    {now, "Australia/Adelaide"},
    {now, "Australia/ACT"},
    {now, "Australia/Melbourne"},
    {now, "Australia/Tasmania"},
    {now, "Australia/Darwin"},
    {now, "Australia/Brisbane"},
    {now, "Australia/LHI"},
    {now, "Australia/NSW"},
    {now, "Australia/Queensland"},
    {now, "Australia/Lord_Howe"},
    {now, "NZ"},
  };
  gint i, nr_of_cases;

  nr_of_cases = sizeof(test_cases) / sizeof (test_cases[0]);
  for (i = 0; i < nr_of_cases; i++)
    assert_time_zone(test_cases[i]);
}

Test(zone, test_logstamp_format)
{
  LogStamp stamp;
  GString *target = g_string_sized_new(32);
  TimestampFormatTestCase test_cases[] =
  {
    /* formats */
    {TS_FMT_BSD, 3600, 3, "Oct 14 20:47:37.123"},
    {TS_FMT_ISO, 3600, 3, "2005-10-14T20:47:37.123+01:00"},
    {TS_FMT_FULL, 3600, 3, "2005 Oct 14 20:47:37.123"},
    {TS_FMT_UNIX, 3600, 3, "1129319257.123"},
    /* time_zone offsets */
    {TS_FMT_ISO, 5400, 3, "2005-10-14T21:17:37.123+01:30"},
    {TS_FMT_ISO, -3600, 3, "2005-10-14T18:47:37.123-01:00"},
    {TS_FMT_ISO, -5400, 3, "2005-10-14T18:17:37.123-01:30"},
  };
  TimestampFormatTestCase boundary_test_case = {TS_FMT_ISO, -1, 0, "1970-01-01T00:00:00+00:00"};
  gint i, nr_of_cases;

  stamp.tv_sec = 1129319257;
  stamp.tv_usec = 123456;
  stamp.zone_offset = 0;
  nr_of_cases = sizeof(test_cases) / sizeof(test_cases[0]);
  for (i = 0; i < nr_of_cases; i++)
    assert_timestamp_format(target, &stamp, test_cases[i]);

  // boundary testing
  stamp.tv_sec = 0;
  stamp.tv_usec = 0;
  assert_timestamp_format(target, &stamp, boundary_test_case);

  g_string_free(target, TRUE);
}
