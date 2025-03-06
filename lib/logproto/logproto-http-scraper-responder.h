/*
 * Copyright (c) 2025 One Identity
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
#ifndef LOGPROTO_HTTP_SCRAPER_RESPONDER_INCLUDED
#define LOGPROTO_HTTP_SCRAPER_RESPONDER_INCLUDED

#include "logproto/logproto-http-server.h"

/* Stat type flags */
#define STT_STAT  0x01
#define STT_QUERY 0x02

/* options */
typedef struct _LogProtoHTTPScraperResponderOptions LogProtoHTTPScraperResponderOptions;
struct _LogProtoHTTPScraperResponderOptions
{
  LogProtoServerOptions /* LogProtoHTTPServerOptions */ super;
  guint8 stat_type;
  gboolean initialized;
};

/* LogProtoHTTPScraperResponder */
typedef struct _LogProtoHTTPScraperResponder LogProtoHTTPScraperResponder;
struct _LogProtoHTTPScraperResponder
{
  LogProtoHTTPServer super;
  LogProtoServerOptions/*Storage*/ *options;
};

void log_proto_http_scraper_responder_options_defaults(LogProtoHTTPScraperResponderOptions *options);
void log_proto_http_scraper_responder_options_init(LogProtoHTTPScraperResponderOptions *options,
                                                   GlobalConfig *cfg);
void log_proto_http_scraper_responder_destroy(LogProtoHTTPScraperResponderOptions *options);

LogProtoServer *log_proto_http_scraper_responder_server_new(
  LogTransport *transport,
  const LogProtoServerOptions *options);
void log_proto_http_scraper_responder_server_init(LogProtoHTTPScraperResponder *self,
                                                  LogTransport *transport,
                                                  const LogProtoServerOptions *options);

#endif
