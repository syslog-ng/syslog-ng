/*
 * Copyright (c) 2020 One Identity
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

#ifndef AZURE_AUTH_HEADER_H
#define AZURE_AUTH_HEADER_H

#include "driver.h"

typedef struct _AzureAuthHeaderPlugin AzureAuthHeaderPlugin;

AzureAuthHeaderPlugin *azure_auth_header_plugin_new(void);

void azure_auth_header_secret_set_from_b64str(AzureAuthHeaderPlugin *self, const gchar *b64secret);
void azure_auth_header_workspace_id_set(AzureAuthHeaderPlugin *self, const gchar *workspace_id);
void azure_auth_header_method_set(AzureAuthHeaderPlugin *self, const gchar *method);
void azure_auth_header_path_set(AzureAuthHeaderPlugin *self, const gchar *path);
void azure_auth_header_content_type_set(AzureAuthHeaderPlugin *self, const gchar *content_type);

#endif
