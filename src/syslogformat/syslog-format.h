#ifndef SYSLOG_FORMAT_H_INCLUDED
#define SYSLOG_FORMAT_H_INCLUDED

#include "msg-format.h"

void syslog_format_handler(MsgFormatOptions *parse_options,
                           const guchar *data, gsize length,
                           LogMessage *self);

#endif
