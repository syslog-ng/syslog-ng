/*
 * Copyright (c) 2012 Nagy, Attila <bra@fsn.hu>
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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

#ifndef AFAMQP_H_INCLUDED
#define AFAMQP_H_INCLUDED

#include "driver.h"
#include "value-pairs/value-pairs.h"

LogDriver *afamqp_dd_new(GlobalConfig *cfg);

void afamqp_dd_set_host(LogDriver *d, const gchar *host);
void afamqp_dd_set_port(LogDriver *d, gint port);
void afamqp_dd_set_exchange(LogDriver *d, const gchar *database);
void afamqp_dd_set_exchange_declare(LogDriver *d, gboolean declare);
void afamqp_dd_set_exchange_type(LogDriver *d, const gchar *exchange_type);
void afamqp_dd_set_vhost(LogDriver *d, const gchar *vhost);
void afamqp_dd_set_routing_key(LogDriver *d, const gchar *routing_key);
void afamqp_dd_set_body(LogDriver *d, const gchar *body);
void afamqp_dd_set_persistent(LogDriver *d, gboolean persistent);
gboolean afamqp_dd_set_auth_method(LogDriver *d, const gchar *auth_method);
void afamqp_dd_set_user(LogDriver *d, const gchar *user);
void afamqp_dd_set_password(LogDriver *d, const gchar *password);
void afamqp_dd_set_max_channel(LogDriver *d, gint max_channel);
void afamqp_dd_set_frame_size(LogDriver *d, gint frame_size);
void afamqp_dd_set_value_pairs(LogDriver *d, ValuePairs *vp);
void afamqp_dd_set_ca_file(LogDriver *d, const gchar *cacrt);
void afamqp_dd_set_key_file(LogDriver *d, const gchar *key);
void afamqp_dd_set_cert_file(LogDriver *d, const gchar *usercrt);
void afamqp_dd_set_peer_verify(LogDriver *d, gboolean verify);


LogTemplateOptions *afamqp_dd_get_template_options(LogDriver *s);

#endif
