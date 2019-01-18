/*
 * Copyright (c) 2018 One Identity
 * Copyright (c) 2018 Balazs Scheidler
 * Copyright (c) 2016 Marc Falzon
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

#ifndef HTTP_WORKER_H_INCLUDED
#define HTTP_WORKER_H_INCLUDED 1

#include "logthrdest/logthrdestdrv.h"
#include "http-loadbalancer.h"

#define CURL_NO_OLDIES 1
#include <curl/curl.h>


typedef struct _HTTPDestinationWorker
{
  LogThreadedDestWorker super;
  HTTPLoadBalancerClient lbc;
  CURL *curl;
  GString *request_body;
  struct curl_slist *request_headers;
} HTTPDestinationWorker;

LogThreadedResult default_map_http_status_to_worker_status(HTTPDestinationWorker *self, const gchar *url,
                                                           glong http_code);
LogThreadedDestWorker *http_dw_new(LogThreadedDestDriver *owner, gint worker_index);

#endif
