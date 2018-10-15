/*
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

#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED 1

#define HTTP_DEFAULT_URL "http://localhost/"
#define METHOD_TYPE_POST 1
#define METHOD_TYPE_PUT  2

#include "logthrdestdrv.h"

#define CURL_NO_OLDIES 1
#include <curl/curl.h>

typedef struct _HTTPDestinationWorker
{
  LogThreadedDestWorker super;
  CURL *curl;
  GString *request_body;
  struct curl_slist *request_headers;
} HTTPDestinationWorker;

typedef struct
{
  LogThreadedDestDriver super;
  gchar *url;
  gchar *user;
  gchar *password;
  GList *headers;
  gchar *user_agent;
  gchar *ca_dir;
  gchar *ca_file;
  gchar *cert_file;
  gchar *key_file;
  gchar *ciphers;
  GString *body_prefix;
  GString *body_suffix;
  GString *delimiter;
  int ssl_version;
  gboolean peer_verify;
  short int method_type;
  glong timeout;
  glong flush_bytes;
  LogTemplate *body_template;
  LogTemplateOptions template_options;
} HTTPDestinationDriver;

gboolean http_dd_init(LogPipe *s);
gboolean http_dd_deinit(LogPipe *s);
LogDriver *http_dd_new(GlobalConfig *cfg);
void http_dd_set_url(LogDriver *d, const gchar *url);
void http_dd_set_user(LogDriver *d, const gchar *user);
void http_dd_set_password(LogDriver *d, const gchar *password);
void http_dd_set_method(LogDriver *d, const gchar *method);
void http_dd_set_user_agent(LogDriver *d, const gchar *user_agent);
void http_dd_set_headers(LogDriver *d, GList *headers);
void http_dd_set_body(LogDriver *d, LogTemplate *body);
void http_dd_set_ca_dir(LogDriver *d, const gchar *ca_dir);
void http_dd_set_ca_file(LogDriver *d, const gchar *ca_file);
void http_dd_set_cert_file(LogDriver *d, const gchar *cert_file);
void http_dd_set_key_file(LogDriver *d, const gchar *key_file);
void http_dd_set_cipher_suite(LogDriver *d, const gchar *ciphers);
void http_dd_set_ssl_version(LogDriver *d, const gchar *value);
void http_dd_set_peer_verify(LogDriver *d, gboolean verify);
void http_dd_set_timeout(LogDriver *d, glong timeout);
void http_dd_set_flush_bytes(LogDriver *d, glong flush_bytes);
void http_dd_set_body_prefix(LogDriver *d, const gchar *body_prefix);
void http_dd_set_body_suffix(LogDriver *d, const gchar *body_suffix);
void http_dd_set_delimiter(LogDriver *d, const gchar *delimiter);
LogTemplateOptions *http_dd_get_template_options(LogDriver *d);

/* internal api */
worker_insert_result_t map_http_status_to_worker_status(HTTPDestinationWorker *self, glong http_code);
HTTPDestinationWorker *http_dw_new(HTTPDestinationDriver *owner, gint worker_index);

#endif
