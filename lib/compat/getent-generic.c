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

#include "compat/getent-generic.h"

#ifndef SYSLOG_NG_HAVE_GETPROTOBYNUMBER_R

#include <glib.h>
#include <errno.h>

G_LOCK_DEFINE_STATIC(getproto);

/* this code does not support proto aliases, as we wouldn't be using
 * them anyway.  Should we ever want to support it, we would need to
 * suballocate @buf and store all of the aliases in the same character
 * array.
 */
static void
_extract_protoent_fields(struct protoent *dst, struct protoent *src, char *buf, size_t buflen)
{
  g_strlcpy(buf, src->p_name, buflen);
  dst->p_name = buf;
  dst->p_aliases = NULL;
  dst->p_proto = src->p_proto;
}

int
_compat_generic__getprotobynumber_r(int proto,
                                    struct protoent *result_buf, char *buf,
                                    size_t buflen, struct protoent **result)
{
  struct protoent *pe;

  G_LOCK(getproto);
  pe = getprotobynumber(proto);

  if (pe)
    {
      _extract_protoent_fields(result_buf, pe, buf, buflen);
      *result = result_buf;
      errno = 0;
    }

  G_UNLOCK(getproto);
  return errno;
}

int
_compat_generic__getprotobyname_r(const char *name,
                                  struct protoent *result_buf, char *buf,
                                  size_t buflen, struct protoent **result)
{
  struct protoent *pe;

  G_LOCK(getproto);
  pe = getprotobyname(name);

  if (pe)
    {
      _extract_protoent_fields(result_buf, pe, buf, buflen);
      *result = result_buf;
      errno = 0;
    }

  G_UNLOCK(getproto);
  return errno;
}

G_LOCK_DEFINE_STATIC(getserv);

/* this code does not support service aliases or using the s_proto field, as
 * we wouldn't be using them anyway.  Should we ever want to support it, we
 * would need to suballocate @buf and store all of the aliases in the same
 * character array.
 */
static void
_extract_servent_fields(struct servent *dst, struct servent *src, char *buf, size_t buflen)
{
  g_strlcpy(buf, src->s_name, buflen);
  dst->s_name = buf;
  dst->s_aliases = NULL;
  dst->s_port = src->s_port;
  /* we don't support s_proto */
  dst->s_proto = NULL;
}


int
_compat_generic__getservbyport_r(int port, const char *proto,
                                 struct servent *result_buf, char *buf,
                                 size_t buflen, struct servent **result)
{
  struct servent *se;

  G_LOCK(getserv);
  se = getservbyport(port, proto);

  if (se)
    {
      _extract_servent_fields(result_buf, se, buf, buflen);
      *result = result_buf;
      errno = 0;
    }

  G_UNLOCK(getserv);
  return errno;
}

int
_compat_generic__getservbyname_r(const char *name, const char *proto,
                                 struct servent *result_buf, char *buf,
                                 size_t buflen, struct servent **result)
{
  struct servent *se;

  G_LOCK(getserv);
  se = getservbyname(name, proto);

  if (se)
    {
      _extract_servent_fields(result_buf, se, buf, buflen);
      *result = result_buf;
      errno = 0;
    }

  G_UNLOCK(getserv);
  return errno;
}

#endif
