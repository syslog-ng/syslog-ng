/*
 * Copyright (c) 2023 Attila Szakacs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "cloud-auth.hpp"

gboolean
cloud_authenticator_init(CloudAuthenticator *s)
{
  g_assert(s->init);
  g_assert(!s->cpp);

  if (!s->init(s))
    return FALSE;

  g_assert(s->cpp);

  return TRUE;
}

void
cloud_authenticator_deinit(CloudAuthenticator *s)
{
  if (s->cpp)
    delete s->cpp;
}

void
cloud_authenticator_free(CloudAuthenticator *s)
{
  if (!s)
    return;

  if (s->free_fn)
    s->free_fn(s);

  g_free(s);
}

void
cloud_authenticator_handle_http_header_request(CloudAuthenticator *s, HttpHeaderRequestSignalData *data)
{
  s->cpp->handle_http_header_request(data);
}
