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
#include "syslog-ng.h"
#include <string.h>

struct sl_name sl_severities[] =
{
  {"emerg",     SEVERITY_CODE(0) },
  {"emergency", SEVERITY_CODE(0) },
  {"panic",     SEVERITY_CODE(0) },
  {"alert",     SEVERITY_CODE(1) },
  {"crit",      SEVERITY_CODE(2) },
  {"critical",  SEVERITY_CODE(2) },
  {"err",       SEVERITY_CODE(3) },
  {"error",     SEVERITY_CODE(3) },
  {"warning",   SEVERITY_CODE(4) },
  {"warn",      SEVERITY_CODE(4) },
  {"notice",    SEVERITY_CODE(5) },
  {"info",      SEVERITY_CODE(6) },
  {"informational", SEVERITY_CODE(6) },
  {"debug",     SEVERITY_CODE(7) },
  {NULL, -1}
};


struct sl_name sl_facilities[] =
{
  {"kern",      FACILITY_CODE(0)  },
  {"user",      FACILITY_CODE(1)  },
  {"mail",      FACILITY_CODE(2)  },
  {"daemon",    FACILITY_CODE(3)  },
  {"auth",      FACILITY_CODE(4)  },
  {"syslog",    FACILITY_CODE(5)  },
  {"lpr",       FACILITY_CODE(6)  },
  {"news",      FACILITY_CODE(7)  },
  {"uucp",      FACILITY_CODE(8)  },
  {"cron",      FACILITY_CODE(9)  },
  {"authpriv",  FACILITY_CODE(10) },
  {"megasafe",  FACILITY_CODE(10) }, /* DEC UNIX AdvFS logging */
  {"ftp",       FACILITY_CODE(11) },
  {"ntp",       FACILITY_CODE(12) },
  {"security",  FACILITY_CODE(13) },
  {"console",   FACILITY_CODE(14) },
  {"solaris-cron",  FACILITY_CODE(15) },

  {"local0",    FACILITY_CODE(16) },
  {"local1",    FACILITY_CODE(17) },
  {"local2",    FACILITY_CODE(18) },
  {"local3",    FACILITY_CODE(19) },
  {"local4",    FACILITY_CODE(20) },
  {"local5",    FACILITY_CODE(21) },
  {"local6",    FACILITY_CODE(22) },
  {"local7",    FACILITY_CODE(23) },
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
