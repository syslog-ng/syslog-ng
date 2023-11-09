/*
 * Copyright (c) 2023 Attila Szakacs
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

#ifndef COMPAT_CURL_H_INCLUDED
#define COMPAT_CURL_H_INCLUDED

#include "compat/compat.h"
#include <curl/curl.h>

#ifndef CURL_VERSION_BITS
#define CURL_VERSION_BITS(x,y,z) ((x)<<16|(y)<<8|(z))
#endif

#ifndef CURL_AT_LEAST_VERSION
#define CURL_AT_LEAST_VERSION(x,y,z) \
  (LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(x, y, z))
#endif

#if !SYSLOG_NG_HAVE_DECL_CURLUE_LAST
#define CURLUE_LAST (64)
#endif

#define SYSLOG_NG_CURL_FULLY_SUPPORTS_URL_PARSING \
  ( \
    SYSLOG_NG_HAVE_DECL_CURL_URL && \
    SYSLOG_NG_HAVE_DECL_CURLU_ALLOW_SPACE && \
    SYSLOG_NG_HAVE_DECL_CURLUE_BAD_SCHEME && \
    SYSLOG_NG_HAVE_DECL_CURLUE_BAD_HOSTNAME && \
    SYSLOG_NG_HAVE_DECL_CURLUE_BAD_PORT_NUMBER && \
    SYSLOG_NG_HAVE_DECL_CURLUE_BAD_USER && \
    SYSLOG_NG_HAVE_DECL_CURLUE_BAD_PASSWORD && \
    SYSLOG_NG_HAVE_DECL_CURLUE_MALFORMED_INPUT && \
    SYSLOG_NG_HAVE_DECL_CURLUPART_SCHEME && \
    SYSLOG_NG_HAVE_DECL_CURLUPART_HOST && \
    SYSLOG_NG_HAVE_DECL_CURLUPART_PORT && \
    SYSLOG_NG_HAVE_DECL_CURLUPART_USER && \
    SYSLOG_NG_HAVE_DECL_CURLUPART_PASSWORD && \
    SYSLOG_NG_HAVE_DECL_CURLUPART_URL \
  )

#endif
