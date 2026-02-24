/*
 * Copyright (c) 2025 Databricks
 * Copyright (c) 2025 David Tosovic <david.tosovic@databricks.com>
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

#ifndef OAUTH2_AUTH_H
#define OAUTH2_AUTH_H

#include "cloud-auth.h"

#include "compat/cpp-start.h"

typedef enum
{
  OAUTH2_AUTH_METHOD_BASIC,
  OAUTH2_AUTH_METHOD_POST_BODY,
  OAUTH2_AUTH_METHOD_CUSTOM
} OAuth2AuthMethod;

CloudAuthenticator *oauth2_authenticator_new(void);
void oauth2_authenticator_set_client_id(CloudAuthenticator *s, const gchar *client_id);
void oauth2_authenticator_set_client_secret(CloudAuthenticator *s, const gchar *client_secret);
void oauth2_authenticator_set_token_url(CloudAuthenticator *s, const gchar *token_url);
void oauth2_authenticator_set_scope(CloudAuthenticator *s, const gchar *scope);
void oauth2_authenticator_set_resource(CloudAuthenticator *s, const gchar *resource);
void oauth2_authenticator_set_authorization_details(CloudAuthenticator *s, const gchar *authorization_details);
void oauth2_authenticator_set_refresh_offset(CloudAuthenticator *s, guint64 refresh_offset_seconds);
void oauth2_authenticator_set_auth_method(CloudAuthenticator *s, OAuth2AuthMethod auth_method);

#include "compat/cpp-end.h"

#endif

