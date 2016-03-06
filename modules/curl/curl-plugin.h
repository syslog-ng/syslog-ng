/*
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

#ifndef SYSLOG_NG_CURL_CURL_H_INCLUDED
#define SYSLOG_NG_CURL_CURL_H_INCLUDED 1

#define CURL_DEFAULT_URL "http://localhost/"

#include "logthrdestdrv.h"

typedef struct
{
  LogThrDestDriver super;
  gchar *curl;
  gchar *url;
  gchar *user;
  gchar *password;
  GList *headers;
  gchar user_agent[1024];
  LogTemplate *body_template;
  LogTemplateOptions template_options;
} CurlDestinationDriver;

gboolean curl_dd_init(LogPipe *s);
gboolean curl_dd_deinit(LogPipe *s);
LogDriver *curl_dd_new(GlobalConfig *cfg);
void curl_dd_set_url(LogDriver *d, const gchar *url);
void curl_dd_set_user(LogDriver *d, const gchar *user);
void curl_dd_set_password(LogDriver *d, const gchar *password);
void curl_dd_set_user_agent(LogDriver *d, const gchar *user_agent);
void curl_dd_set_headers(LogDriver *d, GList *headers);
void curl_dd_set_body(LogDriver *d, LogTemplate *body);
LogTemplateOptions *curl_dd_get_template_options(LogDriver *d);

#endif
