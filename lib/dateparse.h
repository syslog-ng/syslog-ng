#ifndef SYSLOGFORMAT_DATERPARSER_H_INCLUDED
#define SYSLOGFORMAT_DATERPARSER_H_INCLUDED

#include "logmsg.h"
#include "msg-format.h"

gboolean log_msg_parse_date(LogMessage *self, const guchar **data, gint *length, guint parse_flags, glong assume_timezone);

#endif
