/*
 * Copyright (c) 2013, 2014 Balabit
 * Copyright (c) 2013, 2014, 2015 Gergely Nagy
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

#ifndef SNG_RIEMANN_H_INCLUDED
#define SNG_RIEMANN_H_INCLUDED

#include "logthrdest/logthrdestdrv.h"
#include "value-pairs/value-pairs.h"

#include <riemann/riemann-client.h>

typedef struct
{
  LogThreadedDestDriver super;

  gchar *server;
  gint port;
  riemann_client_type_t type;
  guint timeout;

  struct
  {
    LogTemplate *host;
    LogTemplate *service;
    LogTemplate *event_time;
    gint event_time_unit;
    LogTemplate *state;
    LogTemplate *description;
    LogTemplate *metric;
    LogTemplate *ttl;
    GList *tags;
    ValuePairs *attributes;
  } fields;
  LogTemplateOptions template_options;

  struct
  {
    gchar *cacert;
    gchar *cert;
    gchar *key;
  } tls;

} RiemannDestDriver;


LogDriver *riemann_dd_new(GlobalConfig *cfg);

void riemann_dd_set_server(LogDriver *d, const gchar *host);
void riemann_dd_set_port(LogDriver *d, gint port);
LogTemplateOptions *riemann_dd_get_template_options(LogDriver *d);

void riemann_dd_set_field_host(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_service(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_event_time(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_state(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_description(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_metric(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_ttl(LogDriver *d, LogTemplate *value);
void riemann_dd_set_field_tags(LogDriver *d, GList *taglist);
void riemann_dd_set_field_attributes(LogDriver *d, ValuePairs *vp);
gboolean riemann_dd_set_connection_type(LogDriver *d, const gchar *type);
void riemann_dd_set_tls_cacert(LogDriver *d, const gchar *path);
void riemann_dd_set_tls_cert(LogDriver *d, const gchar *path);
void riemann_dd_set_tls_key(LogDriver *d, const gchar *path);
void riemann_dd_set_timeout(LogDriver *d, guint timeout);
void riemann_dd_set_event_time_unit(LogDriver *d, gint unit);

#endif
