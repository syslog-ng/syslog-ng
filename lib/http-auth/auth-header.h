/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@balabit.com>
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

#ifndef HTTP_AUTH_HEADER_H
#define HTTP_AUTH_HEADER_H

#include "syslog-ng.h"

typedef struct _HttpAuthHeader HttpAuthHeader;

struct _HttpAuthHeader
{
  gboolean (*init)(HttpAuthHeader *self);
  void (*free_fn)(HttpAuthHeader *self);

  gboolean (*renew)(HttpAuthHeader *self);
  const gchar *(*get_as_string)(HttpAuthHeader *self);
};

static inline gboolean
http_auth_header_init(HttpAuthHeader *self)
{
  if (!self->init)
    return TRUE;

  return self->init(self);
}

static inline void
http_auth_header_free(HttpAuthHeader *self)
{
  if (self && self->free_fn)
    {
      self->free_fn(self);
    }
}

static inline const gchar *
http_auth_header_get_as_string(HttpAuthHeader *self)
{
  g_assert(self->get_as_string);

  return self->get_as_string(self);
}

static inline gboolean
http_auth_header_renew(HttpAuthHeader *self)
{
  g_assert(self->renew);

  return self->renew(self);
}

#endif

