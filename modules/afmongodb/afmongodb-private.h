/*
 * Copyright (c) 2010-2016 Balabit
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

#ifndef AFMONGODB_PRIVATE_H_
#define AFMONGODB_PRIVATE_H_

#include "syslog-ng.h"
#include "mongoc.h"
#include "logthrdest/logthrdestdrv.h"
#include "string-list.h"
#include "value-pairs/value-pairs.h"

#if SYSLOG_NG_ENABLE_LEGACY_MONGODB_OPTIONS
#include "host-list.h"
#endif

typedef struct _MongoDBDestDriver
{
  LogThreadedDestDriver super;

  /* Shared between main/writer; only read by the writer, never
   written */
  gchar *coll;
  GString *uri_str;

#if SYSLOG_NG_ENABLE_LEGACY_MONGODB_OPTIONS
  GList *servers;
  gchar *address;
  gint port;

  gboolean safe_mode;
  gchar *user;
  gchar *password;
#endif

  LogTemplateOptions template_options;

  ValuePairs *vp;

  /* Writer-only stuff */
#if SYSLOG_NG_ENABLE_LEGACY_MONGODB_OPTIONS
  HostList *recovery_cache;
  gboolean is_legacy;
  gchar *db;
#endif

  const gchar *const_db;
  mongoc_uri_t *uri_obj;
  mongoc_client_t *client;
  mongoc_collection_t *coll_obj;

  GString *current_value;
  bson_t *bson;
} MongoDBDestDriver;

#endif
