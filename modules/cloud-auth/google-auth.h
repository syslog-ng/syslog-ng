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

#ifndef GOOGLE_AUTH_H
#define GOOGLE_AUTH_H

#include "cloud-auth.h"

typedef enum _GoogleAuthenticatorAuthMode
{
  GAAM_UNDEFINED,
  GAAM_SERVICE_ACCOUNT,
} GoogleAuthenticatorAuthMode;

CloudAuthenticator *google_authenticator_new(void);
void google_authenticator_set_auth_mode(CloudAuthenticator *s, GoogleAuthenticatorAuthMode auth_mode);
void google_authenticator_set_service_account_key_path(CloudAuthenticator *s, const gchar *key_path);
void google_authenticator_set_service_account_audience(CloudAuthenticator *s, const gchar *audience);

#endif
