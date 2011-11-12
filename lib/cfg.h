/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
  
#ifndef CFG_H_INCLUDED
#define CFG_H_INCLUDED

#include "syslog-ng.h"
#include "cfg-lexer.h"
#include "cfg-parser.h"
#include "persist-state.h"
#include "templates.h"

#include <sys/types.h>
#include <regex.h>
#include <stdio.h>

struct _LogSourceGroup;
struct _LogDestGroup;
struct _LogProcessRule;
struct _LogConnection;
struct _LogCenter;
struct _LogTemplate;

#define CFG_CURRENT_VERSION 0x0303
#define CFG_CURRENT_VERSION_STRING "3.3"

/* configuration data kept between configuration reloads */
typedef struct _PersistConfig PersistConfig;

/* configuration data as loaded from the config file */
struct _GlobalConfig
{
  /* version number of the configuration file, hex-encoded syslog-ng major/minor, e.g. 0x0201 is syslog-ng 2.1 format */
  gint version;
  gint parsed_version;
  gchar *filename;
  GList *plugins;
  CfgLexer *lexer;

  gint stats_freq;
  gint stats_level;
  gint mark_freq;
  gint flush_lines;
  gint flush_timeout;
  gboolean threaded;
  gboolean chain_hostnames;
  gboolean normalize_hostnames;
  gboolean keep_hostname;
  gboolean check_hostname;
  gboolean bad_hostname_compiled;
  regex_t bad_hostname;
  gchar *bad_hostname_re;
  gboolean use_fqdn;
  gboolean use_dns;
  gboolean use_dns_cache;
  gint dns_cache_size, dns_cache_expire, dns_cache_expire_failed;
  gchar *dns_cache_hosts;
  gint time_reopen;
  gint time_reap;
  gint suppress;

  gint log_fifo_size;
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

  gchar *recv_time_zone;
  LogTemplateOptions template_options;
  
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

gboolean cfg_add_source(GlobalConfig *configuration, struct _LogSourceGroup *group);
gboolean cfg_add_dest(GlobalConfig *configuration, struct _LogDestGroup *group);
gboolean cfg_add_filter(GlobalConfig *configuration, struct _LogProcessRule *rule);
gboolean cfg_add_parser(GlobalConfig *cfg, struct _LogProcessRule *rule);
gboolean cfg_add_rewrite(GlobalConfig *cfg, struct _LogProcessRule *rule);
void cfg_add_connection(GlobalConfig *configuration, struct _LogConnection *conn);
gboolean cfg_add_template(GlobalConfig *cfg, struct _LogTemplate *template);
LogTemplate *cfg_lookup_template(GlobalConfig *cfg, const gchar *name);
LogTemplate *cfg_check_inline_template(GlobalConfig *cfg, const gchar *template_or_name, GError **error);
gboolean cfg_allow_config_dups(GlobalConfig *self);

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
GlobalConfig *cfg_new(gint version);
gboolean cfg_run_parser(GlobalConfig *self, CfgLexer *lexer, CfgParser *parser, gpointer *result, gpointer arg);
gboolean cfg_read_config(GlobalConfig *cfg, gchar *fname, gboolean syntax_only, gchar *preprocess_into);
void cfg_free(GlobalConfig *self);
gboolean cfg_init(GlobalConfig *cfg);
gboolean cfg_deinit(GlobalConfig *cfg);


PersistConfig *persist_config_new(void);
void persist_config_free(PersistConfig *self);
void cfg_persist_config_move(GlobalConfig *src, GlobalConfig *dest);
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
