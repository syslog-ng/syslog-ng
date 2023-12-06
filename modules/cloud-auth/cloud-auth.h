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

#ifndef CLOUD_AUTH_H
#define CLOUD_AUTH_H

#include "syslog-ng.h"

#include "compat/cpp-start.h"

#include "logqueue.h"
#include "driver.h"

#include "modules/http/http-signals.h"

/* Authenticator interface */

typedef struct _CloudAuthenticator CloudAuthenticator;

gboolean cloud_authenticator_init(CloudAuthenticator *s);
void cloud_authenticator_deinit(CloudAuthenticator *s);
void cloud_authenticator_free(CloudAuthenticator *s);
void cloud_authenticator_handle_http_header_request(CloudAuthenticator *s, HttpHeaderRequestSignalData *data);

/* Plugins */

typedef struct _CloudAuthDestPlugin CloudAuthDestPlugin;

LogDriverPlugin *cloud_auth_dest_plugin_new(void);
void cloud_auth_dest_plugin_set_authenticator(LogDriverPlugin *s, CloudAuthenticator *authenticator);

#include "compat/cpp-end.h"


#endif
