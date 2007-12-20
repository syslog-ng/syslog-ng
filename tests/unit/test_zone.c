#include "misc.h"
#include "apphook.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void 
testcase(const gchar *zone, time_t utc, long expected_ofs)
{
  char envbuf[32];
  long ofs;
  
  snprintf(envbuf, sizeof(envbuf), "TZ=%s", zone);
  putenv(envbuf);
  tzset();
  
  ofs = get_local_timezone_ofs(utc);
  if (ofs != expected_ofs)
    {
      fprintf(stderr, "Timezone offset mismatch: %ld, expected %ld\n", ofs, expected_ofs);
      exit(1);
    }
}

int main()
{
  app_startup();
  /* 2005-10-14 21:47:37 CEST, dst enabled */
  testcase("CET", 1129319257, 7200);
  /* 2005-11-14 10:10:00 CET, dst disabled */
  testcase("CET", 1131959400, 3600);

  /* 2005-10-14 21:47:37 GMT, no DST */
  testcase("GMT", 1129319257, 0);
  /* 2005-11-14 10:10:00 GMT, no DST */
  testcase("GMT", 1131959400, 0);

  /* 2005-04-03 01:30:00 ESD, DST disabled */
  testcase("US/Eastern", 1112509800, -5*3600);
  /* 2005-04-03 01:59:59 ESD, DST disabled */
  testcase("US/Eastern", 1112511599, -5*3600);
  /* 2005-04-03 03:00:00 EDT, DST enabled */
  testcase("US/Eastern", 1112511600, -4*3600);
  /* 2005-04-03 03:00:01 EDT, DST enabled */
  testcase("US/Eastern", 1112511601, -4*3600);
  /* 2005-10-30 01:59:59 EDT, DST enabled */
  testcase("US/Eastern", 1130651999, -4*3600);
  /* 2005-10-30 01:00:00 EST, DST disabled */
  testcase("US/Eastern", 1130652000, -5*3600);

  /* USA DST change in 2007, 2nd sunday of March instead of 1st Sunday of April */
  /* 2007-03-11 01:00:00 EST, DST disabled */
  testcase("US/Eastern", 1173592800, -5*3600);
  /* 2007-03-11 01:59:59 EST, DST disabled */
  testcase("US/Eastern", 1173596399, -5*3600);
  /* 2007-03-11 03:00:00 EST, DST enabled */
  testcase("US/Eastern", 1173596400, -4*3600);
  /* 2007-11-04 01:59:59 EST, DST enabled */
  testcase("US/Eastern", 1194155999, -4*3600);
  /* 2007-11-04 01:00:00 EST, DST disabled */
  testcase("US/Eastern", 1194156000, -5*3600);

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

  app_shutdown();
  return 0;
}
