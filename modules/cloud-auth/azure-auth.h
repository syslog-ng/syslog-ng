/*
 * Copyright (c) 2025 Axoflow
 * Copyright (c) 2025 Tamas Kosztyu <tamas.kosztyu@axoflow.com>
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

#ifndef AZURE_AUTH_H
#define AZURE_AUTH_H

#include "cloud-auth.h"

#include "compat/cpp-start.h"

#include "compat/curl.h"

typedef enum _AzureAuthenticatorAuthMode
{
  AAAM_UNDEFINED,
  AAAM_MONITOR,
} AzureAuthenticatorAuthMode;

CloudAuthenticator *azure_authenticator_new(void);

void azure_authenticator_set_auth_mode(CloudAuthenticator *s, AzureAuthenticatorAuthMode auth_mode);

void azure_authenticator_set_tenant_id(CloudAuthenticator *s, const gchar *tenant_id);
void azure_authenticator_set_app_id(CloudAuthenticator *s, const gchar *app_id);
void azure_authenticator_set_scope(CloudAuthenticator *s, const gchar *scope);
void azure_authenticator_set_app_secret(CloudAuthenticator *s, const gchar *app_secret);

#include "compat/cpp-end.h"

#endif