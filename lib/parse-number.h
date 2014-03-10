#ifndef PARSE_NUMBER_H_INCLUDED
#define PARSE_NUMBER_H_INCLUDED

#include "syslog-ng.h"

gboolean parse_number_with_suffix(const gchar *str, gint64 *result);
gboolean parse_number(const gchar *str, gint64 *result);

#endif
