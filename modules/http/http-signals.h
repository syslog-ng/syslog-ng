/*
 * Copyright (c) 2002-2019 One Identity
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@oneidentity.com>
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

#ifndef SIGNAL_SLOT_CONNECTOR_HTTP_SIGNALS_H_INCLUDED
#define SIGNAL_SLOT_CONNECTOR_HTTP_SIGNALS_H_INCLUDED

#include "signal-slot-connector/signal-slot-connector.h"
#include "list-adt.h"

typedef struct _HttpHeaderRequestSignalData HttpHeaderRequestSignalData;

struct _HttpHeaderRequestSignalData
{
  List *request_headers;
  GString *request_body;
};

#define signal_http_header_request SIGNAL(http, header_request, HttpHeaderRequestSignalData *)

#endif
