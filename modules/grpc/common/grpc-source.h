/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#ifndef GRPC_SOURCE_H
#define GRPC_SOURCE_H

#include "syslog-ng.h"

#include "compat/cpp-start.h"

#include "driver.h"
#include "credentials/grpc-credentials-builder.h"

typedef struct GrpcSourceDriver_ GrpcSourceDriver;

void grpc_sd_set_port(LogDriver *s, guint64 port);
void grpc_sd_set_fetch_limit(LogDriver *s, gint fetch_limit);
void grpc_sd_set_concurrent_requests(LogDriver *s, gint concurrent_requests);
void grpc_sd_add_int_channel_arg(LogDriver *s, const gchar *name, gint64 value);
void grpc_sd_add_string_channel_arg(LogDriver *s, const gchar *name, const gchar *value);
GrpcServerCredentialsBuilderW *grpc_sd_get_credentials_builder(LogDriver *s);

#include "compat/cpp-end.h"

#endif
