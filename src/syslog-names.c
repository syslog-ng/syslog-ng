/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "syslog-names.h"
#include "syslog-ng.h"
#include <string.h>

struct sl_name sl_levels[] = {
  {"emerg", LOG_EMERG},
  {"panic", LOG_EMERG},		/* DEPRECATED */
  {"alert", LOG_ALERT},
  {"crit", LOG_CRIT},
  {"err", LOG_ERR},
  {"error", LOG_ERR},		/* DEPRECATED */
  {"warning", LOG_WARNING},
  {"warn", LOG_WARNING},	/* DEPRECATED */
  {"notice", LOG_NOTICE},
  {"info", LOG_INFO},
  {"debug", LOG_DEBUG},
  {NULL, -1}
};

struct sl_name sl_facilities[] = {
  {"auth", LOG_AUTH},
#ifdef LOG_AUTHPRIV
  {"authpriv", LOG_AUTHPRIV},
#endif
#ifdef LOG_CRON
  {"cron", LOG_CRON},
#endif
  {"daemon", LOG_DAEMON},
#ifdef LOG_FTP
  {"ftp", LOG_FTP},
#endif
  {"kern", LOG_KERN},
  {"lpr", LOG_LPR},
  {"mail", LOG_MAIL},
  {"news", LOG_NEWS},
  {"security", LOG_AUTH},	/* DEPRECATED */
  {"syslog", LOG_SYSLOG},
  {"user", LOG_USER},
  {"uucp", LOG_UUCP},
  {"local0", LOG_LOCAL0},
  {"local1", LOG_LOCAL1},
  {"local2", LOG_LOCAL2},
  {"local3", LOG_LOCAL3},
  {"local4", LOG_LOCAL4},
  {"local5", LOG_LOCAL5},
  {"local6", LOG_LOCAL6},
  {"local7", LOG_LOCAL7},
  {NULL, -1}
};

static inline int
syslog_name_find_name(const char *name, struct sl_name names[])
{
  int i;
  
  for (i = 0; names[i].name; i++)
    {
      if (strcasecmp(name, names[i].name) == 0)
	{
	  return i;
	}
    }
  return -1;
}

int
syslog_name_lookup_id_by_name(const char *name, struct sl_name names[])
{
  return syslog_name_find_name(name, names);
}

int
syslog_name_lookup_value_by_name(const char *name, struct sl_name names[])
{
  int i;
  
  i = syslog_name_find_name(name, names);
  if (i != -1)
    {
      return names[i].value;
    }
  return -1;
}

const char *
syslog_name_lookup_name_by_value(int value, struct sl_name names[])
{
  int i;

  for (i = 0; names[i].name; i++)
    {
      if (names[i].value == value)
	{
	  return names[i].name;
	}
    }
  return NULL;
}

guint32
syslog_make_range(guint32 value1, guint32 value2)
{
  guint32 x;

  if (value1 > value2)
    {
      x = value2;
      value2 = value1;
      value1 = x;
    }
  return ((1 << (value2 + 1)) - 1) & ~((1 << value1) - 1);
}
