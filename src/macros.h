#ifndef MACROS_H_INCLUDED
#define MACROS_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"

/* macro expansion flags */
#define MF_ESCAPE_RESULT  0x0001
#define MF_STAMP_RECVD    0x0002

/* macro IDs */
#define M_NONE     0

#define M_FACILITY 1
#define M_LEVEL    2
#define M_TAG      3

#define M_DATE     4
#define M_FULLDATE 5
#define M_ISODATE  6
#define M_YEAR     7
#define M_MONTH    8 
#define M_DAY      9
#define M_HOUR     10
#define M_MIN      11
#define M_SEC      12
#define M_WEEKDAY  13
#define M_TZOFFSET 14
#define M_TZ       15
#define M_UNIXTIME 16

#define M_DATE_RECVD     17
#define M_FULLDATE_RECVD 18
#define M_ISODATE_RECVD  19
#define M_YEAR_RECVD     20
#define M_MONTH_RECVD    21
#define M_DAY_RECVD      22
#define M_HOUR_RECVD     23
#define M_MIN_RECVD      24
#define M_SEC_RECVD      25
#define M_WEEKDAY_RECVD  26
#define M_TZOFFSET_RECVD 27
#define M_TZ_RECVD       28
#define M_UNIXTIME_RECVD 29

#define M_DATE_STAMP     30
#define M_FULLDATE_STAMP 31
#define M_ISODATE_STAMP  32
#define M_YEAR_STAMP     33
#define M_MONTH_STAMP    34
#define M_DAY_STAMP      35
#define M_HOUR_STAMP     36
#define M_MIN_STAMP      37
#define M_SEC_STAMP      38
#define M_WEEKDAY_STAMP  39
#define M_TZOFFSET_STAMP 40
#define M_TZ_STAMP       41
#define M_UNIXTIME_STAMP 42

#define M_FULLHOST       43
#define M_HOST           44
#define M_FULLHOST_FROM  45
#define M_HOST_FROM      46
#define M_PROGRAM        47

#define M_MESSAGE        48

#define M_SOURCE_IP      49

#define M_MATCH_REF_OFS 256

guint
log_macro_lookup(gchar *macro, gint len);

gboolean
log_macro_expand(GString *result, gint id, guint32 flags, glong zone_offset, LogMessage *msg);

#endif
