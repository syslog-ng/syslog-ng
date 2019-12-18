/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef SNMPDEST_H_INCLUDED
#define SNMPDEST_H_INCLUDED

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "driver.h"
#include "mainloop-worker.h"

extern const gchar *s_v2c,
       *s_v3;

typedef struct
{
  LogDestDriver super;

  gchar *version;
  gchar *host;
  GString *host_port;
  gint port;
  GList *snmp_objs; /* the values are stored as string triplets (oid, type, value) - the list size is always dividable by 3 */
  GList *snmp_templates;
  GList *snmp_codes;
  gchar *trap_oid,
        *trap_type,
        *trap_value;
  gchar *community;
  gchar *engine_id;
  gchar *auth_username;
  gchar *auth_algorithm;
  gchar *auth_password;
  gchar *enc_algorithm;
  gchar *enc_password;
  gchar *transport;

  StatsCounterItem *dropped_messages;
  StatsCounterItem *queued_messages;
  StatsCounterItem *processed_messages;

  GMutex *queue_mutex;
  GCond *writer_thread_wakeup_cond;

  gboolean writer_thread_terminate;
  gboolean writer_thread_suspended;

  netsnmp_session session,
                  *ss;
  gboolean session_initialized;
  LogQueue *queue;
  LogTemplate *message;
  LogTemplateOptions template_options;
  WorkerOptions worker_options;

  StatsClusterKey sc_key_queued;
  StatsClusterKey sc_key_dropped;
  StatsClusterKey sc_key_processed;

} SNMPDestDriver;

LogDriver *snmpdest_dd_new(GlobalConfig *cfg);

void snmpdest_dd_set_version(LogDriver *d, const gchar *version);
void snmpdest_dd_set_host(LogDriver *d, const gchar *host);
void snmpdest_dd_set_port(LogDriver *d, gint port);
gboolean snmpdest_dd_set_snmp_obj(LogDriver *d, GlobalConfig *cfg, const gchar *objectid, const gchar *type,
                                  const gchar *value);
void snmpdest_dd_set_trap_obj(LogDriver *d, GlobalConfig *cfg, const gchar *objectid, const gchar *type,
                              const gchar *value);
void snmpdest_dd_set_community(LogDriver *d, const gchar *community);
void snmpdest_dd_set_engine_id(LogDriver *d, const gchar *eid);
void snmpdest_dd_set_auth_username(LogDriver *d, const gchar *auth_username);
void snmpdest_dd_set_auth_algorithm(LogDriver *d, const gchar *auth_algo);
void snmpdest_dd_set_auth_password(LogDriver *d, const gchar *auth_pwd);
void snmpdest_dd_set_enc_algorithm(LogDriver *d, const gchar *enc_algo);
void snmpdest_dd_set_enc_password(LogDriver *d, const gchar *epwd);
void snmpdest_dd_set_transport(LogDriver *d, const gchar *transport);
void snmpdest_dd_set_time_zone(LogDriver *d, const gchar *Time_zone);

gboolean snmpdest_check_required_params(LogDriver *, gchar *err_msg);
gchar *snmpdest_dd_get_version(LogDriver *);
gboolean snmpdest_dd_check_auth_algorithm(gchar *algo);
gboolean snmpdest_dd_check_enc_algorithm(gchar *algo);

#endif
