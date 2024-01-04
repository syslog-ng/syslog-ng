/*
 * Copyright (c) 2002-2010 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "syslog-names.h"
#include <string.h>

struct sl_name sl_severities[] =
{
  {"emerg",     SYSLOG_SEVERITY_CODE(0) },
  {"emergency", SYSLOG_SEVERITY_CODE(0) },
  {"panic",     SYSLOG_SEVERITY_CODE(0) },
  {"alert",     SYSLOG_SEVERITY_CODE(1) },
  {"crit",      SYSLOG_SEVERITY_CODE(2) },
  {"critical",  SYSLOG_SEVERITY_CODE(2) },
  {"err",       SYSLOG_SEVERITY_CODE(3) },
  {"error",     SYSLOG_SEVERITY_CODE(3) },
  {"warning",   SYSLOG_SEVERITY_CODE(4) },
  {"warn",      SYSLOG_SEVERITY_CODE(4) },
  {"notice",    SYSLOG_SEVERITY_CODE(5) },
  {"info",      SYSLOG_SEVERITY_CODE(6) },
  {"informational", SYSLOG_SEVERITY_CODE(6) },
  {"debug",     SYSLOG_SEVERITY_CODE(7) },
  {NULL, -1}
};


struct sl_name sl_facilities[] =
{
  {"kern",      SYSLOG_FACILITY_CODE(0)  },
  {"user",      SYSLOG_FACILITY_CODE(1)  },
  {"mail",      SYSLOG_FACILITY_CODE(2)  },
  {"daemon",    SYSLOG_FACILITY_CODE(3)  },
  {"auth",      SYSLOG_FACILITY_CODE(4)  },
  {"syslog",    SYSLOG_FACILITY_CODE(5)  },
  {"lpr",       SYSLOG_FACILITY_CODE(6)  },
  {"news",      SYSLOG_FACILITY_CODE(7)  },
  {"uucp",      SYSLOG_FACILITY_CODE(8)  },
  {"cron",      SYSLOG_FACILITY_CODE(9)  },
  {"authpriv",  SYSLOG_FACILITY_CODE(10) },
  {"megasafe",  SYSLOG_FACILITY_CODE(10) }, /* DEC UNIX AdvFS logging */
  {"ftp",       SYSLOG_FACILITY_CODE(11) },
  {"ntp",       SYSLOG_FACILITY_CODE(12) },
  {"security",  SYSLOG_FACILITY_CODE(13) },
  {"console",   SYSLOG_FACILITY_CODE(14) },
  {"solaris-cron",  SYSLOG_FACILITY_CODE(15) },

  {"local0",    SYSLOG_FACILITY_CODE(16) },
  {"local1",    SYSLOG_FACILITY_CODE(17) },
  {"local2",    SYSLOG_FACILITY_CODE(18) },
  {"local3",    SYSLOG_FACILITY_CODE(19) },
  {"local4",    SYSLOG_FACILITY_CODE(20) },
  {"local5",    SYSLOG_FACILITY_CODE(21) },
  {"local6",    SYSLOG_FACILITY_CODE(22) },
  {"local7",    SYSLOG_FACILITY_CODE(23) },
  {NULL, -1}
};

static inline gint
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

gint
syslog_name_lookup_id_by_name(const char *name, struct sl_name names[])
{
  return syslog_name_find_name(name, names);
}

gint
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

#include "severity-aliases.h"

gint
syslog_name_lookup_severity_by_name_alias(const gchar *name, gssize name_len)
{
  const struct severity_alias *sa = gperf_lookup_severity_alias(name, name_len < 0 ? strlen(name) : name_len);
  if (sa)
    return sa->severity;
  return -1;
}
