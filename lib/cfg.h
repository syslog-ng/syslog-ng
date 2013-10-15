/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "cfg-tree.h"
#include "cfg-lexer.h"
#include "cfg-parser.h"
#include "persist-state.h"
#include "template/templates.h"
#include "type-hinting.h"

#include <sys/types.h>
#include <regex.h>

/* destination mark modes */
enum
{
  MM_INTERNAL = 1,
  MM_DST_IDLE,
  MM_HOST_IDLE,
  MM_PERIODICAL,
  MM_NONE,
  MM_GLOBAL,
};

/* configuration data kept between configuration reloads */
typedef struct _PersistConfig PersistConfig;

/* configuration data as loaded from the config file */
struct _GlobalConfig
{
  /* version number specified by the user, set _after_ parsing is complete */
  /* hex-encoded syslog-ng major/minor, e.g. 0x0201 is syslog-ng 2.1 format */
  gint user_version;
  
  /* version number as parsed from the configuration file, it can be set
   * multiple times if the user uses @version multiple times */
  gint parsed_version;
  const gchar *filename;
  GList *plugins;
  GList *candidate_plugins;
  gboolean autoload_compiled_modules;
  CfgLexer *lexer;

  gint stats_freq;
  gint stats_level;
  gint mark_freq;
  gint flush_lines;
  gint mark_mode;
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
  gint type_cast_strictness;

  gint log_fifo_size;
  gint log_msg_size;

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
  
  LogTemplate *file_template;
  LogTemplate *proto_template;
  
  PersistConfig *persist;
  PersistState *state;
  
  CfgTree tree;

};

gboolean cfg_allow_config_dups(GlobalConfig *self);

void cfg_file_owner_set(GlobalConfig *self, gchar *owner);
void cfg_file_group_set(GlobalConfig *self, gchar *group);
void cfg_file_perm_set(GlobalConfig *self, gint perm);
void cfg_bad_hostname_set(GlobalConfig *self, gchar *bad_hostname_re);
gint cfg_lookup_mark_mode(gchar *mark_mode);
void cfg_set_mark_mode(GlobalConfig *self, gchar *mark_mode);

void cfg_dir_owner_set(GlobalConfig *self, gchar *owner);
void cfg_dir_group_set(GlobalConfig *self, gchar *group);
void cfg_dir_perm_set(GlobalConfig *self, gint perm);
gint cfg_tz_convert_value(gchar *convert);
gint cfg_ts_format_value(gchar *format);

void cfg_set_version(GlobalConfig *self, gint version);
void cfg_load_candidate_modules(GlobalConfig *self);

GlobalConfig *cfg_new(gint version);
gboolean cfg_run_parser(GlobalConfig *self, CfgLexer *lexer, CfgParser *parser, gpointer *result, gpointer arg);
gboolean cfg_read_config(GlobalConfig *cfg, const gchar *fname, gboolean syntax_only, gchar *preprocess_into);
void cfg_free(GlobalConfig *self);
gboolean cfg_init(GlobalConfig *cfg);
gboolean cfg_deinit(GlobalConfig *cfg);


PersistConfig *persist_config_new(void);
void persist_config_free(PersistConfig *self);
void cfg_persist_config_move(GlobalConfig *src, GlobalConfig *dest);
void cfg_persist_config_add(GlobalConfig *cfg, gchar *name, gpointer value, GDestroyNotify destroy, gboolean force);
gpointer cfg_persist_config_fetch(GlobalConfig *cfg, gchar *name);

static inline gboolean
cfg_is_config_version_older(GlobalConfig *cfg, gint req)
{
  if (!cfg)
    return FALSE;
  if (version_convert_from_user(cfg->user_version) >= req)
    return FALSE;
  return TRUE;
}

#endif
