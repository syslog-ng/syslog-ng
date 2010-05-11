#ifndef SYSLOG_PARSE_H_INCLUDED
#define SYSLOG_PARSE_H_INCLUDED

#include "syslog-ng.h"
#include "timeutils.h"

#include <regex.h>

enum
{
  /* don't parse the message, put everything into $MSG */
  LP_NOPARSE         = 0x0001,
  /* check if the hostname contains valid characters and assume it is part of the program field if it isn't */
  LP_CHECK_HOSTNAME  = 0x0002,
  /* message is using RFC5424 format */
  LP_SYSLOG_PROTOCOL = 0x0004,
  /* the caller knows the message is valid UTF-8 */
  LP_ASSUME_UTF8     = 0x0008,
  /* validate that all characters are indeed UTF-8 and mark the message as valid when relaying */
  LP_VALIDATE_UTF8   = 0x0010,
  /* the message may not contain NL characters, strip them if it does */
  LP_NO_MULTI_LINE   = 0x0020,
  /* don't store MSGHDR in the LEGACY_MSGHDR macro */
  LP_STORE_LEGACY_MSGHDR = 0x0040,
  /* expect a hostname field in the message */
  LP_EXPECT_HOSTNAME = 0x0080,
};

typedef struct _LogParseOptions
{
  gchar *recv_time_zone;
  TimeZoneInfo *recv_time_zone_info;
  regex_t *bad_hostname;
  guint32 flags;
  guint16 default_pri;
} LogParseOptions;


void log_parse_syslog(LogParseOptions *parse_options, const guchar *data, gint length, LogMessage *msg);
void log_parse_syslog_options_defaults(LogParseOptions *options);
void log_parse_syslog_options_init(LogParseOptions *parse_options, GlobalConfig *cfg);
void log_parse_syslog_options_destroy(LogParseOptions *parse_options);

#endif
