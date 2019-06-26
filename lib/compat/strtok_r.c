/*
 * Copyright (c) 2002-2013 Balabit
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
#include "compat/string.h"

#if !SYSLOG_NG_HAVE_STRTOK_R || defined(TEST_STRTOK_R)

char *
strtok_r(char *str, const char *delim, char **saveptr)
{
  char *it;
  char *head;

  if (str)
    *saveptr = str;

  if (!*saveptr)
    return NULL;

  it = *saveptr;

  /*find the first non-delimiter*/
  it += strspn(it, delim);

  head = it;

  if (!it || !*it)
    {
      *saveptr = NULL;
      return NULL;
    }

  /* find the first delimiter */
  it = strpbrk(it, delim);
  /* skip all the delimiters */
  while (it && *it && strchr(delim, *it))
    {
      *it = '\0';
      it++;
    }

  *saveptr = it;

  return head;
}
#endif
