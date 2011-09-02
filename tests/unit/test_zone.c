#include "misc.h"
#include "apphook.h"
#include "timeutils.h"
#include "timeutils.h"
#include "logstamp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void
set_tz(const char *tz)
{
  static char envbuf[64];

  snprintf(envbuf, sizeof(envbuf), "TZ=%s", tz);
  putenv(envbuf);
  tzset();
  clean_time_cache();
}

void
testcase(const gchar *zone, time_t utc, long expected_ofs)
{
  long ofs;

  set_tz(zone);

  ofs = get_local_timezone_ofs(utc);
  if (ofs != expected_ofs)
    {
      fprintf(stderr, "Timezone offset mismatch: zone: %s, %ld, expected %ld\n", zone, ofs, expected_ofs);
      exit(1);
    }
}

int
timezone_exists(const char *time_zone)
{
  TimeZoneInfo *info;

  set_tz(time_zone);
  info = time_zone_info_new(time_zone);

  if (info)
    {
      time_zone_info_free(info);
      return TRUE;
    }
  return FALSE;
}

int
test_timezone_2(const time_t stamp_to_test, const char* time_zone)
{
  TimeZoneInfo *info;
  time_t offset, expected_offset;

  set_tz(time_zone);
  info = time_zone_info_new(time_zone);
  offset = time_zone_info_get_offset(info, stamp_to_test);
  expected_offset = get_local_timezone_ofs(stamp_to_test);
  if (offset != expected_offset)
    printf("unixtimestamp: %ld TimeZoneName (%s) localtime offset(%ld), timezone file offset(%ld)\n", (glong) stamp_to_test, time_zone, (glong) expected_offset, (glong) offset);
  time_zone_info_free(info);
  return offset == expected_offset;
}

int
test_timezone(const time_t stamp_to_test, const char* time_zone)
{
  if (!timezone_exists(time_zone))
    {
      printf("SKIP: %s\n", time_zone);
      return TRUE;
    }
  return test_timezone_2(stamp_to_test, time_zone) &&
         test_timezone_2(stamp_to_test - 6*30*24*60*60, time_zone) &&
         test_timezone_2(stamp_to_test + 6*30*24*6, time_zone);
}

#define TEST_ASSERT(x)  \
  do { \
   if (!(x)) \
     { \
       fprintf(stderr, "test assertion failed: " #x " line: %d\n", __LINE__); \
       rc = 1; \
     } \
  } while (0)


int
test_zones(void)
{
  gint rc = 0;
  time_t now;

  /* 2005-10-14 21:47:37 CEST, dst enabled */
  testcase("MET-1METDST", 1129319257, 7200);
  /* 2005-11-14 10:10:00 CET, dst disabled */
  testcase("MET-1METDST", 1131959400, 3600);

  /* 2005-10-14 21:47:37 GMT, no DST */
  testcase("GMT", 1129319257, 0);
  /* 2005-11-14 10:10:00 GMT, no DST */
  testcase("GMT", 1131959400, 0);

  /* 2005-04-03 01:30:00 ESD, DST disabled */
  testcase("EST5EDT", 1112509800, -5*3600);
  /* 2005-04-03 01:59:59 ESD, DST disabled */
  testcase("EST5EDT", 1112511599, -5*3600);
  /* 2005-04-03 03:00:00 EDT, DST enabled */
  testcase("EST5EDT", 1112511600, -4*3600);
  /* 2005-04-03 03:00:01 EDT, DST enabled */
  testcase("EST5EDT", 1112511601, -4*3600);
  /* 2005-10-30 01:59:59 EDT, DST enabled */
  testcase("EST5EDT", 1130651999, -4*3600);
  /* 2005-10-30 01:00:00 EST, DST disabled */
  testcase("EST5EDT", 1130652000, -5*3600);

#ifdef __linux__

  /* NOTE: not all of our build systems have been upgraded to work correctly
   * with post 2007 years. Therefore we restrict their tests to Linux which
   * work ok. */

  /* USA DST change in 2007, 2nd sunday of March instead of 1st Sunday of April */
  /* 2007-03-11 01:00:00 EST, DST disabled */
  testcase("EST5EDT", 1173592800, -5*3600);
  /* 2007-03-11 01:59:59 EST, DST disabled */
  testcase("EST5EDT", 1173596399, -5*3600);
# if __GLIBC__ && __GLIBC_MINOR__ > 3
  /* Except on legacy systems lacking updated timezone info.
   * Like Debian Potato ... */
  /* 2007-03-11 03:00:00 EST, DST enabled */
  testcase("EST5EDT", 1173596400, -4*3600);
  /* 2007-11-04 01:59:59 EST, DST enabled */
  testcase("EST5EDT", 1194155999, -4*3600);
# endif
  /* 2007-11-04 01:00:00 EST, DST disabled */
  testcase("EST5EDT", 1194156000, -5*3600);
#endif

#ifdef __linux__
  /* Oct 31 01:59:59 2004 (EST) +1000 */
  testcase("Australia/Victoria", 1099151999, 10*3600);
  /* Oct 31 03:00:00 2004 (EST) +1100 */
  testcase("Australia/Victoria", 1099152000, 11*3600);
  /* Mar 27 02:59:59 2005 (EST) +1100 */
  testcase("Australia/Victoria", 1111852799, 11*3600);
  /* Mar 27 02:00:00 2005 (EST) +1000 */
  testcase("Australia/Victoria", 1111852800, 10*3600);

  /* Oct  2 01:59:59 2004 (NZT) +1200 */
  testcase("NZ", 1128175199, 12*3600);
  /* Oct  2 03:00:00 2004 (NZDT) +1300 */
  testcase("NZ", 1128175200, 13*3600);
  /* Mar 20 02:59:59 2005 (NZDT) +1300 */
  testcase("NZ", 1111240799, 13*3600);
  /* Mar 20 02:00:00 2005 (NZT) +1200 */
  testcase("NZ", 1111240800, 12*3600);

  now = time(NULL);

  TEST_ASSERT(test_timezone(now, "America/Louisville"));
  //TEST_ASSERT(test_timezone(now, "Hongkong"));
  TEST_ASSERT(test_timezone(now, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486860, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486740, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800-1800, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800+1800, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800-3600, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800+3600, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800-3601, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(1288486800+3601, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(now, "Asia/Kuala_Lumpur"));
  TEST_ASSERT(test_timezone(now, "CST6CDT"));
  TEST_ASSERT(test_timezone(now, "US/Pacific"));
  TEST_ASSERT(test_timezone(now, "US/Indiana-Starke"));
  TEST_ASSERT(test_timezone(now, "US/Samoa"));
  TEST_ASSERT(test_timezone(now, "US/Arizona"));
  TEST_ASSERT(test_timezone(now, "US/Aleutian"));
  TEST_ASSERT(test_timezone(now, "US/Michigan"));
  TEST_ASSERT(test_timezone(now, "US/Alaska"));
  TEST_ASSERT(test_timezone(now, "US/Central"));
  TEST_ASSERT(test_timezone(now, "US/Hawaii"));
  TEST_ASSERT(test_timezone(now, "US/East-Indiana"));
  TEST_ASSERT(test_timezone(now, "US/Eastern"));
  TEST_ASSERT(test_timezone(now, "US/Mountain"));
  TEST_ASSERT(test_timezone(now, "Pacific/Noumea"));
  TEST_ASSERT(test_timezone(now, "Pacific/Samoa"));
  TEST_ASSERT(test_timezone(now, "Pacific/Apia"));
  TEST_ASSERT(test_timezone(now, "Pacific/Auckland"));
  TEST_ASSERT(test_timezone(now, "Pacific/Fakaofo"));
  TEST_ASSERT(test_timezone(now, "Pacific/Saipan"));
  TEST_ASSERT(test_timezone(now, "Pacific/Easter"));
  TEST_ASSERT(test_timezone(now, "Pacific/Norfolk"));
  TEST_ASSERT(test_timezone(now, "Pacific/Kosrae"));
  TEST_ASSERT(test_timezone(now, "Pacific/Tarawa"));
  TEST_ASSERT(test_timezone(now, "Pacific/Tahiti"));
  TEST_ASSERT(test_timezone(now, "Pacific/Pago_Pago"));
  TEST_ASSERT(test_timezone(now, "Pacific/Majuro"));
  TEST_ASSERT(test_timezone(now, "Pacific/Guam"));
  TEST_ASSERT(test_timezone(now, "Pacific/Ponape"));
  TEST_ASSERT(test_timezone(now, "Pacific/Tongatapu"));
  TEST_ASSERT(test_timezone(now, "Pacific/Funafuti"));
  TEST_ASSERT(test_timezone(now, "Pacific/Wallis"));
  TEST_ASSERT(test_timezone(now, "Pacific/Efate"));
  TEST_ASSERT(test_timezone(now, "Pacific/Marquesas"));
  TEST_ASSERT(test_timezone(now, "Pacific/Enderbury"));
  TEST_ASSERT(test_timezone(now, "Pacific/Pitcairn"));
  TEST_ASSERT(test_timezone(now, "Pacific/Yap"));
  TEST_ASSERT(test_timezone(now, "Pacific/Wake"));
  TEST_ASSERT(test_timezone(now, "Pacific/Johnston"));
  TEST_ASSERT(test_timezone(now, "Pacific/Kwajalein"));
  TEST_ASSERT(test_timezone(now, "Pacific/Chatham"));
  TEST_ASSERT(test_timezone(now, "Pacific/Gambier"));
  TEST_ASSERT(test_timezone(now, "Pacific/Galapagos"));
  TEST_ASSERT(test_timezone(now, "Pacific/Kiritimati"));
  TEST_ASSERT(test_timezone(now, "Pacific/Honolulu"));
  TEST_ASSERT(test_timezone(now, "Pacific/Truk"));
  TEST_ASSERT(test_timezone(now, "Pacific/Midway"));
  TEST_ASSERT(test_timezone(now, "Pacific/Fiji"));
  TEST_ASSERT(test_timezone(now, "Pacific/Rarotonga"));
  TEST_ASSERT(test_timezone(now, "Pacific/Guadalcanal"));
  TEST_ASSERT(test_timezone(now, "Pacific/Nauru"));
  TEST_ASSERT(test_timezone(now, "Pacific/Palau"));
  TEST_ASSERT(test_timezone(now, "Pacific/Port_Moresby"));
  TEST_ASSERT(test_timezone(now, "Pacific/Niue"));
  TEST_ASSERT(test_timezone(now, "GMT"));
  TEST_ASSERT(test_timezone(now, "Hongkong"));
  TEST_ASSERT(test_timezone(now, "ROK"));
  TEST_ASSERT(test_timezone(now, "Navajo"));
  TEST_ASSERT(test_timezone(now, "ROC"));
  TEST_ASSERT(test_timezone(now, "WET"));
  TEST_ASSERT(test_timezone(now, "posixrules"));
  TEST_ASSERT(test_timezone(now, "CET"));
  TEST_ASSERT(test_timezone(now, "MET"));
  TEST_ASSERT(test_timezone(now, "MST"));
  TEST_ASSERT(test_timezone(now, "Turkey"));
  TEST_ASSERT(test_timezone(now, "Zulu"));
  TEST_ASSERT(test_timezone(now, "GMT+0"));
  TEST_ASSERT(test_timezone(now, "Canada/Saskatchewan"));
  TEST_ASSERT(test_timezone(now, "Canada/Pacific"));
  TEST_ASSERT(test_timezone(now, "Canada/Yukon"));
  TEST_ASSERT(test_timezone(now, "Canada/East-Saskatchewan"));
  TEST_ASSERT(test_timezone(now, "Canada/Newfoundland"));
  TEST_ASSERT(test_timezone(now, "Canada/Central"));
  TEST_ASSERT(test_timezone(now, "Canada/Eastern"));
  TEST_ASSERT(test_timezone(now, "Canada/Atlantic"));
  TEST_ASSERT(test_timezone(now, "Canada/Mountain"));
  TEST_ASSERT(test_timezone(now, "Singapore"));
  TEST_ASSERT(test_timezone(now, "UCT"));
  TEST_ASSERT(test_timezone(now, "Poland"));
  TEST_ASSERT(test_timezone(now, "Chile/Continental"));
  TEST_ASSERT(test_timezone(now, "Chile/EasterIsland"));
  TEST_ASSERT(test_timezone(now, "Portugal"));
  TEST_ASSERT(test_timezone(now, "Egypt"));
  TEST_ASSERT(test_timezone(now, "Japan"));
  TEST_ASSERT(test_timezone(now, "Iran"));
  TEST_ASSERT(test_timezone(now, "EET"));
  TEST_ASSERT(test_timezone(now, "Europe/Oslo"));
  TEST_ASSERT(test_timezone(now, "Europe/Bratislava"));
  TEST_ASSERT(test_timezone(now, "Europe/Gibraltar"));
  TEST_ASSERT(test_timezone(now, "Europe/Skopje"));
  TEST_ASSERT(test_timezone(now, "Europe/Simferopol"));
  TEST_ASSERT(test_timezone(now, "Europe/Zurich"));
  TEST_ASSERT(test_timezone(now, "Europe/Vienna"));
  TEST_ASSERT(test_timezone(now, "Europe/Paris"));
  TEST_ASSERT(test_timezone(now, "Europe/Jersey"));
  TEST_ASSERT(test_timezone(now, "Europe/Tallinn"));
  TEST_ASSERT(test_timezone(now, "Europe/Madrid"));
  TEST_ASSERT(test_timezone(now, "Europe/Volgograd"));
  TEST_ASSERT(test_timezone(now, "Europe/Zaporozhye"));
  TEST_ASSERT(test_timezone(now, "Europe/Mariehamn"));
  TEST_ASSERT(test_timezone(now, "Europe/Vaduz"));
  TEST_ASSERT(test_timezone(now, "Europe/Moscow"));
  TEST_ASSERT(test_timezone(now, "Europe/Stockholm"));
  TEST_ASSERT(test_timezone(now, "Europe/Minsk"));
  TEST_ASSERT(test_timezone(now, "Europe/Andorra"));
  TEST_ASSERT(test_timezone(now, "Europe/Istanbul"));
  TEST_ASSERT(test_timezone(now, "Europe/Tiraspol"));
  TEST_ASSERT(test_timezone(now, "Europe/Podgorica"));
  TEST_ASSERT(test_timezone(now, "Europe/Bucharest"));
  TEST_ASSERT(test_timezone(now, "Europe/Ljubljana"));
  TEST_ASSERT(test_timezone(now, "Europe/Brussels"));
  TEST_ASSERT(test_timezone(now, "Europe/Amsterdam"));
  TEST_ASSERT(test_timezone(now, "Europe/Riga"));
  TEST_ASSERT(test_timezone(now, "Europe/Tirane"));
  TEST_ASSERT(test_timezone(now, "Europe/Berlin"));
  TEST_ASSERT(test_timezone(now, "Europe/Guernsey"));
  TEST_ASSERT(test_timezone(now, "Europe/Warsaw"));
  TEST_ASSERT(test_timezone(now, "Europe/Vatican"));
  TEST_ASSERT(test_timezone(now, "Europe/Malta"));
  TEST_ASSERT(test_timezone(now, "Europe/Nicosia"));
  TEST_ASSERT(test_timezone(now, "Europe/Budapest"));
  TEST_ASSERT(test_timezone(now, "Europe/Kaliningrad"));
  TEST_ASSERT(test_timezone(now, "Europe/Sarajevo"));
  TEST_ASSERT(test_timezone(now, "Europe/Isle_of_Man"));
  TEST_ASSERT(test_timezone(now, "Europe/Rome"));
  TEST_ASSERT(test_timezone(now, "Europe/London"));
  TEST_ASSERT(test_timezone(now, "Europe/Vilnius"));
  TEST_ASSERT(test_timezone(now, "Europe/Samara"));
  TEST_ASSERT(test_timezone(now, "Europe/Belfast"));
  TEST_ASSERT(test_timezone(now, "Europe/Athens"));
  TEST_ASSERT(test_timezone(now, "Europe/Copenhagen"));
  TEST_ASSERT(test_timezone(now, "Europe/Belgrade"));
  TEST_ASSERT(test_timezone(now, "Europe/Sofia"));
  TEST_ASSERT(test_timezone(now, "Europe/San_Marino"));
  TEST_ASSERT(test_timezone(now, "Europe/Helsinki"));
  TEST_ASSERT(test_timezone(now, "Europe/Uzhgorod"));
  TEST_ASSERT(test_timezone(now, "Europe/Monaco"));
  TEST_ASSERT(test_timezone(now, "Europe/Prague"));
  TEST_ASSERT(test_timezone(now, "Europe/Zagreb"));
  TEST_ASSERT(test_timezone(now, "Europe/Lisbon"));
  TEST_ASSERT(test_timezone(now, "Europe/Chisinau"));
  TEST_ASSERT(test_timezone(now, "Europe/Dublin"));
  TEST_ASSERT(test_timezone(now, "Europe/Luxembourg"));
  TEST_ASSERT(test_timezone(now, "Europe/Kiev"));
  TEST_ASSERT(test_timezone(now, "Jamaica"));
  TEST_ASSERT(test_timezone(now, "Indian/Chagos"));
  TEST_ASSERT(test_timezone(now, "Indian/Comoro"));
  TEST_ASSERT(test_timezone(now, "Indian/Mauritius"));
  TEST_ASSERT(test_timezone(now, "Indian/Mayotte"));
  TEST_ASSERT(test_timezone(now, "Indian/Kerguelen"));
  TEST_ASSERT(test_timezone(now, "Indian/Maldives"));
  TEST_ASSERT(test_timezone(now, "Indian/Antananarivo"));
  TEST_ASSERT(test_timezone(now, "Indian/Mahe"));
  TEST_ASSERT(test_timezone(now, "Indian/Cocos"));
  TEST_ASSERT(test_timezone(now, "Indian/Christmas"));
  TEST_ASSERT(test_timezone(now, "Indian/Reunion"));
  TEST_ASSERT(test_timezone(now, "Africa/Accra"));
  TEST_ASSERT(test_timezone(now, "Africa/Lubumbashi"));
  TEST_ASSERT(test_timezone(now, "Africa/Bangui"));
  TEST_ASSERT(test_timezone(now, "Africa/Asmara"));
  TEST_ASSERT(test_timezone(now, "Africa/Freetown"));
  TEST_ASSERT(test_timezone(now, "Africa/Mbabane"));
  TEST_ASSERT(test_timezone(now, "Africa/Djibouti"));
  TEST_ASSERT(test_timezone(now, "Africa/Ndjamena"));
  TEST_ASSERT(test_timezone(now, "Africa/Porto-Novo"));
  TEST_ASSERT(test_timezone(now, "Africa/Nouakchott"));
  TEST_ASSERT(test_timezone(now, "Africa/Brazzaville"));
  TEST_ASSERT(test_timezone(now, "Africa/Tunis"));
  TEST_ASSERT(test_timezone(now, "Africa/Dar_es_Salaam"));
  TEST_ASSERT(test_timezone(now, "Africa/Lusaka"));
  TEST_ASSERT(test_timezone(now, "Africa/Banjul"));
  TEST_ASSERT(test_timezone(now, "Africa/Sao_Tome"));
  TEST_ASSERT(test_timezone(now, "Africa/Monrovia"));
  TEST_ASSERT(test_timezone(now, "Africa/Lagos"));
  TEST_ASSERT(test_timezone(now, "Africa/Conakry"));
  TEST_ASSERT(test_timezone(now, "Africa/Tripoli"));
  TEST_ASSERT(test_timezone(now, "Africa/Algiers"));
  TEST_ASSERT(test_timezone(now, "Africa/Casablanca"));
  TEST_ASSERT(test_timezone(now, "Africa/Lome"));
  TEST_ASSERT(test_timezone(now, "Africa/Bamako"));
  TEST_ASSERT(test_timezone(now, "Africa/Nairobi"));
  TEST_ASSERT(test_timezone(now, "Africa/Douala"));
  TEST_ASSERT(test_timezone(now, "Africa/Addis_Ababa"));
  TEST_ASSERT(test_timezone(now, "Africa/El_Aaiun"));
  TEST_ASSERT(test_timezone(now, "Africa/Ceuta"));
  TEST_ASSERT(test_timezone(now, "Africa/Harare"));
  TEST_ASSERT(test_timezone(now, "Africa/Libreville"));
  TEST_ASSERT(test_timezone(now, "Africa/Johannesburg"));
  TEST_ASSERT(test_timezone(now, "Africa/Timbuktu"));
  TEST_ASSERT(test_timezone(now, "Africa/Niamey"));
  TEST_ASSERT(test_timezone(now, "Africa/Windhoek"));
  TEST_ASSERT(test_timezone(now, "Africa/Bissau"));
  TEST_ASSERT(test_timezone(now, "Africa/Maputo"));
  TEST_ASSERT(test_timezone(now, "Africa/Kigali"));
  TEST_ASSERT(test_timezone(now, "Africa/Dakar"));
  TEST_ASSERT(test_timezone(now, "Africa/Ouagadougou"));
  TEST_ASSERT(test_timezone(now, "Africa/Gaborone"));
  TEST_ASSERT(test_timezone(now, "Africa/Khartoum"));
  TEST_ASSERT(test_timezone(now, "Africa/Bujumbura"));
  TEST_ASSERT(test_timezone(now, "Africa/Luanda"));
  TEST_ASSERT(test_timezone(now, "Africa/Malabo"));
  TEST_ASSERT(test_timezone(now, "Africa/Asmera"));
  TEST_ASSERT(test_timezone(now, "Africa/Maseru"));
  TEST_ASSERT(test_timezone(now, "Africa/Abidjan"));
  TEST_ASSERT(test_timezone(now, "Africa/Kinshasa"));
  TEST_ASSERT(test_timezone(now, "Africa/Blantyre"));
  TEST_ASSERT(test_timezone(now, "Africa/Cairo"));
  TEST_ASSERT(test_timezone(now, "Africa/Kampala"));
  TEST_ASSERT(test_timezone(now, "Africa/Mogadishu"));
  TEST_ASSERT(test_timezone(now, "Universal"));
  TEST_ASSERT(test_timezone(now, "EST"));
  TEST_ASSERT(test_timezone(now, "Greenwich"));
  TEST_ASSERT(test_timezone(now, "Eire"));
  TEST_ASSERT(test_timezone(now, "Asia/Qatar"));
  TEST_ASSERT(test_timezone(now, "Asia/Makassar"));
  TEST_ASSERT(test_timezone(now, "Asia/Colombo"));
  TEST_ASSERT(test_timezone(now, "Asia/Chungking"));
  TEST_ASSERT(test_timezone(now, "Asia/Macau"));
  TEST_ASSERT(test_timezone(now, "Asia/Choibalsan"));
  TEST_ASSERT(test_timezone(now, "Asia/Rangoon"));
  TEST_ASSERT(test_timezone(now, "Asia/Harbin"));
  TEST_ASSERT(test_timezone(now, "Asia/Yerevan"));
  TEST_ASSERT(test_timezone(now, "Asia/Brunei"));
  TEST_ASSERT(test_timezone(now, "Asia/Omsk"));
  TEST_ASSERT(test_timezone(now, "Asia/Chongqing"));
  TEST_ASSERT(test_timezone(now, "Asia/Tbilisi"));
  TEST_ASSERT(test_timezone(now, "Asia/Tehran"));
  TEST_ASSERT(test_timezone(now, "Asia/Manila"));
  TEST_ASSERT(test_timezone(now, "Asia/Yakutsk"));
  TEST_ASSERT(test_timezone(now, "Asia/Qyzylorda"));
  TEST_ASSERT(test_timezone(now, "Asia/Dhaka"));
  TEST_ASSERT(test_timezone(now, "Asia/Singapore"));
  TEST_ASSERT(test_timezone(now, "Asia/Jakarta"));
  TEST_ASSERT(test_timezone(now, "Asia/Novosibirsk"));
  TEST_ASSERT(test_timezone(now, "Asia/Saigon"));
  TEST_ASSERT(test_timezone(now, "Asia/Krasnoyarsk"));
  TEST_ASSERT(test_timezone(now, "Asia/Kabul"));
  TEST_ASSERT(test_timezone(now, "Asia/Bahrain"));
  TEST_ASSERT(test_timezone(now, "Asia/Urumqi"));
  TEST_ASSERT(test_timezone(now, "Asia/Thimbu"));
  TEST_ASSERT(test_timezone(now, "Asia/Ulan_Bator"));
  TEST_ASSERT(test_timezone(now, "Asia/Taipei"));
  TEST_ASSERT(test_timezone(now, "Asia/Tashkent"));
  TEST_ASSERT(test_timezone(now, "Asia/Dacca"));
  TEST_ASSERT(test_timezone(now, "Asia/Aqtau"));
  TEST_ASSERT(test_timezone(now, "Asia/Seoul"));
  TEST_ASSERT(test_timezone(now, "Asia/Istanbul"));
  TEST_ASSERT(test_timezone(now, "Asia/Tel_Aviv"));
  TEST_ASSERT(test_timezone(now, "Asia/Almaty"));
  TEST_ASSERT(test_timezone(now, "Asia/Phnom_Penh"));
  TEST_ASSERT(test_timezone(now, "Asia/Baku"));
  TEST_ASSERT(test_timezone(now, "Asia/Anadyr"));
  TEST_ASSERT(test_timezone(now, "Asia/Aqtobe"));
  TEST_ASSERT(test_timezone(now, "Asia/Kuwait"));
  TEST_ASSERT(test_timezone(now, "Asia/Irkutsk"));
  TEST_ASSERT(test_timezone(now, "Asia/Ulaanbaatar"));
  TEST_ASSERT(test_timezone(now, "Asia/Tokyo"));
  TEST_ASSERT(test_timezone(now, "Asia/Gaza"));
  TEST_ASSERT(test_timezone(now, "Asia/Riyadh87"));
  TEST_ASSERT(test_timezone(now, "Asia/Yekaterinburg"));
  TEST_ASSERT(test_timezone(now, "Asia/Riyadh88"));
  TEST_ASSERT(test_timezone(now, "Asia/Nicosia"));
  TEST_ASSERT(test_timezone(now, "Asia/Jayapura"));
  TEST_ASSERT(test_timezone(now, "Asia/Shanghai"));
  TEST_ASSERT(test_timezone(now, "Asia/Pyongyang"));
  TEST_ASSERT(test_timezone(now, "Asia/Macao"));
  TEST_ASSERT(test_timezone(now, "Asia/Dushanbe"));
  TEST_ASSERT(test_timezone(now, "Asia/Kuching"));
  TEST_ASSERT(test_timezone(now, "Asia/Vientiane"));
  TEST_ASSERT(test_timezone(now, "Asia/Riyadh"));
  TEST_ASSERT(test_timezone(now, "Asia/Dili"));
  TEST_ASSERT(test_timezone(now, "Asia/Samarkand"));
  TEST_ASSERT(test_timezone(now, "Asia/Ashkhabad"));
  TEST_ASSERT(test_timezone(now, "Asia/Calcutta"));
  TEST_ASSERT(test_timezone(now, "Asia/Hong_Kong"));
  TEST_ASSERT(test_timezone(now, "Asia/Sakhalin"));
  TEST_ASSERT(test_timezone(now, "Asia/Beirut"));
  TEST_ASSERT(test_timezone(now, "Asia/Damascus"));
  TEST_ASSERT(test_timezone(now, "Asia/Katmandu"));
  TEST_ASSERT(test_timezone(now, "Asia/Jerusalem"));
  TEST_ASSERT(test_timezone(now, "Asia/Riyadh89"));
  TEST_ASSERT(test_timezone(now, "Asia/Vladivostok"));
  TEST_ASSERT(test_timezone(now, "Asia/Kamchatka"));
  TEST_ASSERT(test_timezone(now, "Asia/Dubai"));
  TEST_ASSERT(test_timezone(now, "Asia/Kashgar"));
  TEST_ASSERT(test_timezone(now, "Asia/Ashgabat"));
  TEST_ASSERT(test_timezone(now, "Asia/Amman"));
  TEST_ASSERT(test_timezone(now, "Asia/Karachi"));
  TEST_ASSERT(test_timezone(now, "Asia/Bangkok"));
  TEST_ASSERT(test_timezone(now, "Asia/Oral"));
  TEST_ASSERT(test_timezone(now, "Asia/Thimphu"));
  TEST_ASSERT(test_timezone(now, "Asia/Bishkek"));
  TEST_ASSERT(test_timezone(now, "Asia/Baghdad"));
  TEST_ASSERT(test_timezone(now, "Asia/Kuala_Lumpur"));
  TEST_ASSERT(test_timezone(now, "Asia/Pontianak"));
  TEST_ASSERT(test_timezone(now, "Asia/Ujung_Pandang"));
  TEST_ASSERT(test_timezone(now, "Asia/Muscat"));
  TEST_ASSERT(test_timezone(now, "Asia/Aden"));
  TEST_ASSERT(test_timezone(now, "Asia/Hovd"));
  TEST_ASSERT(test_timezone(now, "Asia/Magadan"));
  TEST_ASSERT(test_timezone(now, "EST5EDT"));
  TEST_ASSERT(test_timezone(now, "PST8PDT"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Bermuda"));
  TEST_ASSERT(test_timezone(now, "Atlantic/St_Helena"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Cape_Verde"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Stanley"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Azores"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Jan_Mayen"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Reykjavik"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Madeira"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Canary"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Faeroe"));
  TEST_ASSERT(test_timezone(now, "Atlantic/Faroe"));
  TEST_ASSERT(test_timezone(now, "Atlantic/South_Georgia"));
  TEST_ASSERT(test_timezone(now, "Kwajalein"));
  TEST_ASSERT(test_timezone(now, "UTC"));
  TEST_ASSERT(test_timezone(now, "GMT-0"));
  TEST_ASSERT(test_timezone(now, "MST7MDT"));
  TEST_ASSERT(test_timezone(now, "GB-Eire"));
  TEST_ASSERT(test_timezone(now, "PRC"));
  TEST_ASSERT(test_timezone(now, "Arctic/Longyearbyen"));
  TEST_ASSERT(test_timezone(now, "Cuba"));
  TEST_ASSERT(test_timezone(now, "Israel"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-3"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+1"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-5"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-13"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-1"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-9"));
  TEST_ASSERT(test_timezone(now, "Etc/Zulu"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+0"));
  TEST_ASSERT(test_timezone(now, "Etc/UCT"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+12"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+9"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-6"));
  TEST_ASSERT(test_timezone(now, "Etc/Universal"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-2"));
  TEST_ASSERT(test_timezone(now, "Etc/Greenwich"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+3"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+8"));
  TEST_ASSERT(test_timezone(now, "Etc/UTC"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-0"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-14"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+10"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+4"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+5"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-12"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-8"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+7"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-11"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+6"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT0"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-7"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+11"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-4"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT+2"));
  TEST_ASSERT(test_timezone(now, "Etc/GMT-10"));
  TEST_ASSERT(test_timezone(now, "HST"));
  TEST_ASSERT(test_timezone(now, "Iceland"));
  TEST_ASSERT(test_timezone(now, "Mexico/BajaNorte"));
  TEST_ASSERT(test_timezone(now, "Mexico/BajaSur"));
  TEST_ASSERT(test_timezone(now, "Mexico/General"));
  TEST_ASSERT(test_timezone(now, "Mideast/Riyadh87"));
  TEST_ASSERT(test_timezone(now, "Mideast/Riyadh88"));
  TEST_ASSERT(test_timezone(now, "Mideast/Riyadh89"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Davis"));
  TEST_ASSERT(test_timezone(now, "Antarctica/DumontDUrville"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Syowa"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Palmer"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Casey"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Rothera"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Mawson"));
  TEST_ASSERT(test_timezone(now, "Antarctica/McMurdo"));
  TEST_ASSERT(test_timezone(now, "Antarctica/South_Pole"));
  TEST_ASSERT(test_timezone(now, "Antarctica/Vostok"));
  TEST_ASSERT(test_timezone(now, "America/Curacao"));
  TEST_ASSERT(test_timezone(now, "America/St_Lucia"));
  TEST_ASSERT(test_timezone(now, "America/Managua"));
  TEST_ASSERT(test_timezone(now, "America/Lima"));
  TEST_ASSERT(test_timezone(now, "America/Nipigon"));
  TEST_ASSERT(test_timezone(now, "America/Cayenne"));
  TEST_ASSERT(test_timezone(now, "America/Detroit"));
  TEST_ASSERT(test_timezone(now, "America/Kentucky/Louisville"));
  TEST_ASSERT(test_timezone(now, "America/Kentucky/Monticello"));
  TEST_ASSERT(test_timezone(now, "America/Belem"));
  TEST_ASSERT(test_timezone(now, "America/Jujuy"));
  TEST_ASSERT(test_timezone(now, "America/Godthab"));
  TEST_ASSERT(test_timezone(now, "America/Guatemala"));
  TEST_ASSERT(test_timezone(now, "America/Atka"));
  TEST_ASSERT(test_timezone(now, "America/Montreal"));
  TEST_ASSERT(test_timezone(now, "America/Resolute"));
  TEST_ASSERT(test_timezone(now, "America/Thunder_Bay"));
  TEST_ASSERT(test_timezone(now, "America/North_Dakota/New_Salem"));
  TEST_ASSERT(test_timezone(now, "America/North_Dakota/Center"));
  TEST_ASSERT(test_timezone(now, "America/Panama"));
  TEST_ASSERT(test_timezone(now, "America/Los_Angeles"));
  TEST_ASSERT(test_timezone(now, "America/Whitehorse"));
  TEST_ASSERT(test_timezone(now, "America/Santiago"));
  TEST_ASSERT(test_timezone(now, "America/Iqaluit"));
  TEST_ASSERT(test_timezone(now, "America/Dawson"));
  TEST_ASSERT(test_timezone(now, "America/Juneau"));
  TEST_ASSERT(test_timezone(now, "America/Thule"));
  TEST_ASSERT(test_timezone(now, "America/Fortaleza"));
  TEST_ASSERT(test_timezone(now, "America/Montevideo"));
  TEST_ASSERT(test_timezone(now, "America/Tegucigalpa"));
  TEST_ASSERT(test_timezone(now, "America/Port-au-Prince"));
  TEST_ASSERT(test_timezone(now, "America/Guadeloupe"));
  TEST_ASSERT(test_timezone(now, "America/Coral_Harbour"));
  TEST_ASSERT(test_timezone(now, "America/Monterrey"));
  TEST_ASSERT(test_timezone(now, "America/Anguilla"));
  TEST_ASSERT(test_timezone(now, "America/Antigua"));
  TEST_ASSERT(test_timezone(now, "America/Campo_Grande"));
  TEST_ASSERT(test_timezone(now, "America/Buenos_Aires"));
  TEST_ASSERT(test_timezone(now, "America/Maceio"));
  TEST_ASSERT(test_timezone(now, "America/New_York"));
  TEST_ASSERT(test_timezone(now, "America/Puerto_Rico"));
  TEST_ASSERT(test_timezone(now, "America/Noronha"));
  TEST_ASSERT(test_timezone(now, "America/Sao_Paulo"));
  TEST_ASSERT(test_timezone(now, "America/Cancun"));
  TEST_ASSERT(test_timezone(now, "America/Aruba"));
  TEST_ASSERT(test_timezone(now, "America/Yellowknife"));
  TEST_ASSERT(test_timezone(now, "America/Knox_IN"));
  TEST_ASSERT(test_timezone(now, "America/Halifax"));
  TEST_ASSERT(test_timezone(now, "America/Grand_Turk"));
  TEST_ASSERT(test_timezone(now, "America/Vancouver"));
  TEST_ASSERT(test_timezone(now, "America/Bogota"));
  TEST_ASSERT(test_timezone(now, "America/Santo_Domingo"));
  TEST_ASSERT(test_timezone(now, "America/Tortola"));
  TEST_ASSERT(test_timezone(now, "America/Blanc-Sablon"));
  TEST_ASSERT(test_timezone(now, "America/St_Thomas"));
  TEST_ASSERT(test_timezone(now, "America/Scoresbysund"));
  TEST_ASSERT(test_timezone(now, "America/Jamaica"));
  TEST_ASSERT(test_timezone(now, "America/El_Salvador"));
  TEST_ASSERT(test_timezone(now, "America/Montserrat"));
  TEST_ASSERT(test_timezone(now, "America/Martinique"));
  TEST_ASSERT(test_timezone(now, "America/Nassau"));
  TEST_ASSERT(test_timezone(now, "America/Indianapolis"));
  TEST_ASSERT(test_timezone(now, "America/Pangnirtung"));
  TEST_ASSERT(test_timezone(now, "America/Port_of_Spain"));
  TEST_ASSERT(test_timezone(now, "America/Mexico_City"));
  TEST_ASSERT(test_timezone(now, "America/Denver"));
  TEST_ASSERT(test_timezone(now, "America/Dominica"));
  TEST_ASSERT(test_timezone(now, "America/Eirunepe"));
  TEST_ASSERT(test_timezone(now, "America/Atikokan"));
  TEST_ASSERT(test_timezone(now, "America/Glace_Bay"));
  TEST_ASSERT(test_timezone(now, "America/Rainy_River"));
  TEST_ASSERT(test_timezone(now, "America/St_Barthelemy"));
  TEST_ASSERT(test_timezone(now, "America/Miquelon"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Vevay"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Vincennes"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Marengo"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Petersburg"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Tell_City"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Knox"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Indianapolis"));
  TEST_ASSERT(test_timezone(now, "America/Indiana/Winamac"));
  TEST_ASSERT(test_timezone(now, "America/Menominee"));
  TEST_ASSERT(test_timezone(now, "America/Porto_Velho"));
  TEST_ASSERT(test_timezone(now, "America/Phoenix"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/San_Juan"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Jujuy"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Ushuaia"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Buenos_Aires"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/La_Rioja"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/ComodRivadavia"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Tucuman"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Rio_Gallegos"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Mendoza"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Cordoba"));
  TEST_ASSERT(test_timezone(now, "America/Argentina/Catamarca"));
  TEST_ASSERT(test_timezone(now, "America/Dawson_Creek"));
  TEST_ASSERT(test_timezone(now, "America/Merida"));
  TEST_ASSERT(test_timezone(now, "America/Moncton"));
  TEST_ASSERT(test_timezone(now, "America/Goose_Bay"));
  TEST_ASSERT(test_timezone(now, "America/Grenada"));
  TEST_ASSERT(test_timezone(now, "America/Barbados"));
  TEST_ASSERT(test_timezone(now, "America/Tijuana"));
  TEST_ASSERT(test_timezone(now, "America/Regina"));
  TEST_ASSERT(test_timezone(now, "America/Ensenada"));
  TEST_ASSERT(test_timezone(now, "America/Louisville"));
  TEST_ASSERT(test_timezone(now, "America/Edmonton"));
  TEST_ASSERT(test_timezone(now, "America/Bahia"));
  TEST_ASSERT(test_timezone(now, "America/Nome"));
  TEST_ASSERT(test_timezone(now, "America/Guayaquil"));
  TEST_ASSERT(test_timezone(now, "America/La_Paz"));
  TEST_ASSERT(test_timezone(now, "America/Costa_Rica"));
  TEST_ASSERT(test_timezone(now, "America/Mazatlan"));
  TEST_ASSERT(test_timezone(now, "America/Havana"));
  TEST_ASSERT(test_timezone(now, "America/Marigot"));
  TEST_ASSERT(test_timezone(now, "America/Mendoza"));
  TEST_ASSERT(test_timezone(now, "America/Virgin"));
  TEST_ASSERT(test_timezone(now, "America/Manaus"));
  TEST_ASSERT(test_timezone(now, "America/Rosario"));
  TEST_ASSERT(test_timezone(now, "America/Boa_Vista"));
  TEST_ASSERT(test_timezone(now, "America/Winnipeg"));
  TEST_ASSERT(test_timezone(now, "America/Paramaribo"));
  TEST_ASSERT(test_timezone(now, "America/Danmarkshavn"));
  TEST_ASSERT(test_timezone(now, "America/Caracas"));
  TEST_ASSERT(test_timezone(now, "America/Swift_Current"));
  TEST_ASSERT(test_timezone(now, "America/St_Johns"));
  TEST_ASSERT(test_timezone(now, "America/Araguaina"));
  TEST_ASSERT(test_timezone(now, "America/Adak"));
  TEST_ASSERT(test_timezone(now, "America/Cordoba"));
  TEST_ASSERT(test_timezone(now, "America/Fort_Wayne"));
  TEST_ASSERT(test_timezone(now, "America/Catamarca"));
  TEST_ASSERT(test_timezone(now, "America/Recife"));
  TEST_ASSERT(test_timezone(now, "America/Toronto"));
  TEST_ASSERT(test_timezone(now, "America/Anchorage"));
  TEST_ASSERT(test_timezone(now, "America/St_Vincent"));
  TEST_ASSERT(test_timezone(now, "America/St_Kitts"));
  TEST_ASSERT(test_timezone(now, "America/Chihuahua"));
  TEST_ASSERT(test_timezone(now, "America/Cayman"));
  TEST_ASSERT(test_timezone(now, "America/Belize"));
  TEST_ASSERT(test_timezone(now, "America/Cambridge_Bay"));
  TEST_ASSERT(test_timezone(now, "America/Cuiaba"));
  TEST_ASSERT(test_timezone(now, "America/Chicago"));
  TEST_ASSERT(test_timezone(now, "America/Guyana"));
  TEST_ASSERT(test_timezone(now, "America/Inuvik"));
  TEST_ASSERT(test_timezone(now, "America/Asuncion"));
  TEST_ASSERT(test_timezone(now, "America/Porto_Acre"));
  TEST_ASSERT(test_timezone(now, "America/Hermosillo"));
  TEST_ASSERT(test_timezone(now, "America/Shiprock"));
  TEST_ASSERT(test_timezone(now, "America/Rio_Branco"));
  TEST_ASSERT(test_timezone(now, "America/Yakutat"));
  TEST_ASSERT(test_timezone(now, "America/Rankin_Inlet"));
  TEST_ASSERT(test_timezone(now, "America/Boise"));
  TEST_ASSERT(test_timezone(now, "Brazil/West"));
  TEST_ASSERT(test_timezone(now, "Brazil/Acre"));
  TEST_ASSERT(test_timezone(now, "Brazil/East"));
  TEST_ASSERT(test_timezone(now, "Brazil/DeNoronha"));
  TEST_ASSERT(test_timezone(now, "GMT0"));
  TEST_ASSERT(test_timezone(now, "Libya"));
  TEST_ASSERT(test_timezone(now, "W-SU"));
  TEST_ASSERT(test_timezone(now, "NZ-CHAT"));
  TEST_ASSERT(test_timezone(now, "Factory"));
  TEST_ASSERT(test_timezone(now, "GB"));
  TEST_ASSERT(test_timezone(now, "Australia/West"));
  TEST_ASSERT(test_timezone(now, "Australia/Canberra"));
  TEST_ASSERT(test_timezone(now, "Australia/Broken_Hill"));
  TEST_ASSERT(test_timezone(now, "Australia/Eucla"));
  TEST_ASSERT(test_timezone(now, "Australia/Currie"));
  TEST_ASSERT(test_timezone(now, "Australia/South"));
  TEST_ASSERT(test_timezone(now, "Australia/Lindeman"));
  TEST_ASSERT(test_timezone(now, "Australia/Hobart"));
  TEST_ASSERT(test_timezone(now, "Australia/Perth"));
  TEST_ASSERT(test_timezone(now, "Australia/Yancowinna"));
  TEST_ASSERT(test_timezone(now, "Australia/Victoria"));
  TEST_ASSERT(test_timezone(now, "Australia/Sydney"));
  TEST_ASSERT(test_timezone(now, "Australia/North"));
  TEST_ASSERT(test_timezone(now, "Australia/Adelaide"));
  TEST_ASSERT(test_timezone(now, "Australia/ACT"));
  TEST_ASSERT(test_timezone(now, "Australia/Melbourne"));
  TEST_ASSERT(test_timezone(now, "Australia/Tasmania"));
  TEST_ASSERT(test_timezone(now, "Australia/Darwin"));
  TEST_ASSERT(test_timezone(now, "Australia/Brisbane"));
  TEST_ASSERT(test_timezone(now, "Australia/LHI"));
  TEST_ASSERT(test_timezone(now, "Australia/NSW"));
  TEST_ASSERT(test_timezone(now, "Australia/Queensland"));
  TEST_ASSERT(test_timezone(now, "Australia/Lord_Howe"));
  TEST_ASSERT(test_timezone(now, "NZ"));
#endif
  return rc;
}

int
test_logstamp(void)
{
  LogStamp stamp;
  GString *target = g_string_sized_new(32);
  gint rc = 0;

  stamp.tv_sec = 1129319257;
  stamp.tv_usec = 123456;

  /* formats */
  log_stamp_format(&stamp, target, TS_FMT_BSD, 3600, 3);
  TEST_ASSERT(strcmp(target->str, "Oct 14 20:47:37.123") == 0);
  log_stamp_format(&stamp, target, TS_FMT_ISO, 3600, 3);
  TEST_ASSERT(strcmp(target->str, "2005-10-14T20:47:37.123+01:00") == 0);
  log_stamp_format(&stamp, target, TS_FMT_FULL, 3600, 3);
  TEST_ASSERT(strcmp(target->str, "2005 Oct 14 20:47:37.123") == 0);
  log_stamp_format(&stamp, target, TS_FMT_UNIX, 3600, 3);
  TEST_ASSERT(strcmp(target->str, "1129319257.123") == 0);

  /* timezone offsets */
  log_stamp_format(&stamp, target, TS_FMT_ISO, 5400, 3);
  TEST_ASSERT(strcmp(target->str, "2005-10-14T21:17:37.123+01:30") == 0);
  log_stamp_format(&stamp, target, TS_FMT_ISO, -3600, 3);
  TEST_ASSERT(strcmp(target->str, "2005-10-14T18:47:37.123-01:00") == 0);
  log_stamp_format(&stamp, target, TS_FMT_ISO, -5400, 3);
  TEST_ASSERT(strcmp(target->str, "2005-10-14T18:17:37.123-01:30") == 0);
  g_string_free(target, TRUE);
  return rc;
}

int
main(int argc, char *argv[])
{
  gint rc;
  app_startup();

  rc = test_logstamp() | test_zones();;
  app_shutdown();
  return rc;
}
