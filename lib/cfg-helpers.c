/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2024 OneIdentity
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
#include "cfg-helpers.h"

#include <stdio.h>

gboolean
cfg_check_port(const gchar *port)
{
  /* only digits (->numbers) are allowed */
  int len = strlen(port);
  if (len < 1 || len > 5)
    return FALSE;
  for (int i = 0; i < len; ++i)
    if (port[i] < '0' || port[i] > '9')
      return FALSE;
  int port_num = atoi(port);
  return port_num > 0 && port_num <= G_MAXUINT16;
}

int cfg_get_parser_maximum_stack_depth(void)
{
  static int max_depth = -1;

  if (max_depth != -1)
    return max_depth;

#define SYSLOG_NG_CONFIG_MAX_STACK_DEPTH "SYSLOG_NG_CONFIG_MAX_STACK_DEPTH"
  const int default_max_depth = 20000; // the old default YYMAXDEPTH value
  const char *env_value = getenv(SYSLOG_NG_CONFIG_MAX_STACK_DEPTH);

  max_depth = default_max_depth;
  if (env_value != NULL)
    {
      max_depth = atoi(env_value);
      if (max_depth < default_max_depth)
        {
          fprintf(stderr,
                  SYSLOG_NG_CONFIG_MAX_STACK_DEPTH " is set to %d, but it must be at least %d. Using default value %d.\n", max_depth,
                  default_max_depth, default_max_depth);
          max_depth = default_max_depth;
        }
    }
  return max_depth;
}
