/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#ifndef CFG_H_INCLUDED
#define CFG_H_INCLUDED

#include "syslog-ng.h"
#include "cfg-lexer.h"
#include "persist-state.h"

#include <sys/types.h>
#include <regex.h>
#include <stdio.h>

struct _LogSourceGroup;
struct _LogDestGroup;
struct _LogProcessRule;
struct _LogConnection;
struct _LogCenter;
struct _LogTemplate;


/* configuration data kept between configuration reloads */
typedef struct _PersistConfig PersistConfig;

/* configuration data as loaded from the config file */
struct _GlobalConfig
{
  /* version number of the configuration file, hex-encoded syslog-ng major/minor, e.g. 0x0201 is syslog-ng 2.1 format */
  gint version;
  gchar *filename;
  GList *plugins;
  CfgLexer *lexer;

  gint stats_freq;
  gint stats_level;
  gint mark_freq;
  gint flush_lines;
  gint flush_timeout;
  gboolean chain_hostnames;
  gboolean normalize_hostnames;
  gboolean keep_hostname;
  gboolean check_hostname;
  gboolean bad_hostname_compiled;
  regex_t bad_hostname;
  gchar *bad_hostname_re;
  gboolean use_time_recvd;
  gboolean use_fqdn;
  gboolean use_dns;
  gboolean use_dns_cache;
  gint dns_cache_size, dns_cache_expire, dns_cache_expire_failed;
  gchar *dns_cache_hosts;
  gint time_reopen;
  gint time_reap;
  gint time_sleep;

  gint log_fifo_size;
  gint log_fetch_limit;
  gint log_iw_size;
  gint log_msg_size;

  gint follow_freq;
  gboolean create_dirs;
  gint file_uid;
  gint file_gid;
  gint file_perm;
  
  gint dir_uid;
  gint dir_gid;
  gint dir_perm;

  gboolean keep_timestamp;  
  gint ts_format;
  gint frac_digits;

  gchar *recv_time_zone;
  gchar *send_time_zone;
  gchar *local_time_zone;
  
  gchar *file_template_name;
  gchar *proto_template_name;
  
  struct _LogTemplate *file_template;
  struct _LogTemplate *proto_template;
  
  /* */
  GHashTable *sources;
  GHashTable *destinations;
  GHashTable *filters;
  GHashTable *parsers;
  GHashTable *rewriters;
  GHashTable *templates;
  GPtrArray *connections;
  PersistConfig *persist;
  PersistState *state;

  struct _LogCenter *center;
  
};

void cfg_add_source(GlobalConfig *configuration, struct _LogSourceGroup *group);
void cfg_add_dest(GlobalConfig *configuration, struct _LogDestGroup *group);
void cfg_add_filter(GlobalConfig *configuration, struct _LogProcessRule *rule);
void cfg_add_parser(GlobalConfig *cfg, struct _LogProcessRule *rule);
void cfg_add_rewrite(GlobalConfig *cfg, struct _LogProcessRule *rule);
void cfg_add_connection(GlobalConfig *configuration, struct _LogConnection *conn);
void cfg_add_template(GlobalConfig *cfg, struct _LogTemplate *template);
struct _LogTemplate *cfg_lookup_template(GlobalConfig *cfg, const gchar *name);
struct _LogTemplate *cfg_check_inline_template(GlobalConfig *cfg, const gchar *template_or_name);

void cfg_file_owner_set(GlobalConfig *self, gchar *owner);
void cfg_file_group_set(GlobalConfig *self, gchar *group);
void cfg_file_perm_set(GlobalConfig *self, gint perm);
void cfg_bad_hostname_set(GlobalConfig *self, gchar *bad_hostname_re);

void cfg_dir_owner_set(GlobalConfig *self, gchar *owner);
void cfg_dir_group_set(GlobalConfig *self, gchar *group);
void cfg_dir_perm_set(GlobalConfig *self, gint perm);
gint cfg_tz_convert_value(gchar *convert);
gint cfg_ts_format_value(gchar *format);

void cfg_set_version(GlobalConfig *self, gint version);
GlobalConfig *cfg_new(gchar *fname, gboolean syntax_only, gchar *preprocess_into);
void cfg_free(GlobalConfig *self);
gboolean cfg_init(GlobalConfig *cfg);
gboolean cfg_deinit(GlobalConfig *cfg);
GlobalConfig *cfg_reload_config(gchar *fname, GlobalConfig *cfg);
gboolean cfg_initial_init(GlobalConfig *cfg, const gchar *persist_filename);


void cfg_persist_config_add(GlobalConfig *cfg, gchar *name, gpointer value, GDestroyNotify destroy, gboolean force);
gpointer cfg_persist_config_fetch(GlobalConfig *cfg, gchar *name);

static inline gboolean 
cfg_check_current_config_version(gint req)
{
  if (!configuration)
    return TRUE;
  else if (configuration->version >= req)
    return TRUE;
  return FALSE;
}

#endif
