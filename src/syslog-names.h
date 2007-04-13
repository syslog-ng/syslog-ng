#ifndef __SYSLOG_NAMES_H_INCLUDED
#define __SYSLOG_NAMES_H_INCLUDED

#include "syslog-ng.h"
#include <syslog.h>

struct sl_name
{
  char *name;
  int value;
};

extern struct sl_name sl_levels[];
extern struct sl_name sl_facilities[];

/* returns an index where this name is found */
int syslog_lookup_name(const char *name, struct sl_name names[]);
char *syslog_lookup_value(int value, struct sl_name names[]);
guint32 syslog_make_range(guint32 r1, guint32 r2);

#define syslog_lookup_level(name) syslog_lookup_name(name, sl_levels)
#define syslog_lookup_facility(name) syslog_lookup_name(name, sl_facilities)

#endif
