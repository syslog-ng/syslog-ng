/*
 * Copyright (c) 2019 Balabit
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

#ifndef HTTP_RESPONSE_HANDLER_H_INCLUDED
#define HTTP_RESPONSE_HANDLER_H_INCLUDED 1

#include "syslog-ng.h"

typedef enum
{
  HTTP_RESULT_SUCCESS,
  HTTP_RESULT_RETRY,
  HTTP_RESULT_DROP,
  HTTP_RESULT_DISCONNECT,
  HTTP_RESULT_MAX
} HttpResult;

typedef HttpResult (*HttpResponseAction)(gpointer user_data);
HttpResult http_result_success(gpointer user_data);
HttpResult http_result_retry(gpointer user_data);
HttpResult http_result_drop(gpointer user_data);
HttpResult http_result_disconnect(gpointer user_data);

typedef struct
{
  gint status_code;
  HttpResponseAction action;
  gpointer user_data;
} HttpResponseHandler;

#endif
