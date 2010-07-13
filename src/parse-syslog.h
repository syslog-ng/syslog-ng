#ifndef PARSE_SYSLOG_H_INCLUDED
#define PARSE_SYSLOG_H_INCLUDED

#include "msg-format.h"

void log_parse_syslog(LogParseOptions *parse_options, const guchar *data, gint length, LogMessage *msg);

#endif
