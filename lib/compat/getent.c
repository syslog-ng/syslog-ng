/*
 * Copyright (c) 2017 Balabit
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

#if defined(sun) || defined(__sun)

#include "compat/getent-bb.h"
#include <errno.h>

int bb__getprotobynumber_r(int proto,
                           struct protoent *result_buf, char *buf,
                           size_t buflen, struct protoent **result)
{
  *result = getprotobynumber_r(proto, result_buf, buf, buflen);
  return (*result ? NULL : errno);
}

int bb__getprotobyname_r(const char *name,
                         struct protoent *result_buf, char *buf,
                         size_t buflen, struct protoent **result)
{
  *result = getprotobyname_r(name, result_buf, buf, buflen);
  return (*result ? NULL : errno);
}

int bb__getservbyport_r(int port, const char *proto,
                        struct servent *result_buf, char *buf,
                        size_t buflen, struct servent **result)
{
  *result =  getservbyport_r(port, proto, result_buf, buf, buflen);
  return (*result ? NULL : errno);
}

int bb__getservbyname_r(const char *name, const char *proto,
                        struct servent *result_buf, char *buf,
                        size_t buflen, struct servent **result)
{
  *result =  getservbyname_r(name, proto, result_buf, buf, buflen);
  return (*result ? NULL : errno);
}
#endif
