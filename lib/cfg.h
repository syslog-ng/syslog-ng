/*
 * Copyright (c) 2002-2013 Balabit
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
#include "plugin.h"
#include "persist-state.h"
#include "template/templates.h"
#include "host-resolve.h"
#include "type-hinting.h"
#include "stats/stats.h"
#include "dnscache.h"
#include "file-perms.h"

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
  guint parsed_version;
  const gchar *filename;
  PluginContext plugin_context;
  gboolean use_plugin_discovery;
  gboolean enable_forced_modules;
  CfgLexer *lexer;
  CfgArgs *globals;

  StatsOptions stats_options;
  gint mark_freq;
  gint flush_lines;
  gint mark_mode;
  gboolean threaded;
  gboolean pass_unix_credentials;
  gboolean chain_hostnames;
  gboolean keep_hostname;
  gboolean check_hostname;
  gboolean bad_hostname_compiled;
  regex_t bad_hostname;
  gchar *bad_hostname_re;
  gchar *custom_domain;
  DNSCacheOptions dns_cache_options;
  gint time_reopen;
  gint time_reap;
  gint suppress;
  gint type_cast_strictness;

  gint log_fifo_size;
  gint log_msg_size;
  gboolean trim_large_messages;

  gboolean create_dirs;
  FilePermOptions file_perm_options;
  GList *source_mangle_callback_list;
  gboolean use_uniqid;

  gboolean keep_timestamp;

  gchar *recv_time_zone;
  LogTemplateOptions template_options;
  HostResolveOptions host_resolve_options;

  gchar *file_template_name;
  gchar *proto_template_name;

  LogTemplate *file_template;
  LogTemplate *proto_template;

  guint min_iw_size_per_reader;

  PersistConfig *persist;
  PersistState *state;
  GHashTable *module_config;

  CfgTree tree;

  GString *preprocess_config;
  GString *original_config;
};

gboolean cfg_load_module(GlobalConfig *cfg, const gchar *module_name);

Plugin *cfg_find_plugin(GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
gpointer cfg_parse_plugin(GlobalConfig *cfg, Plugin *plugin, YYLTYPE *yylloc, gpointer arg);

gboolean cfg_allow_config_dups(GlobalConfig *self);

void cfg_bad_hostname_set(GlobalConfig *self, gchar *bad_hostname_re);
gint cfg_lookup_mark_mode(const gchar *mark_mode);
void cfg_set_mark_mode(GlobalConfig *self, const gchar *mark_mode);

gint cfg_tz_convert_value(gchar *convert);
gint cfg_ts_format_value(gchar *format);

gboolean cfg_set_version(GlobalConfig *self, gint version);
void cfg_load_candidate_modules(GlobalConfig *self);

void cfg_set_global_paths(GlobalConfig *self);

GlobalConfig *cfg_new(gint version);
GlobalConfig *cfg_new_snippet(void);
gboolean cfg_run_parser(GlobalConfig *self, CfgLexer *lexer, CfgParser *parser, gpointer *result, gpointer arg);
gboolean cfg_read_config(GlobalConfig *cfg, const gchar *fname, gboolean syntax_only, gchar *preprocess_into);
void cfg_load_forced_modules(GlobalConfig *self);
void cfg_shutdown(GlobalConfig *self);
gboolean cfg_is_shutting_down(GlobalConfig *cfg);
void cfg_free(GlobalConfig *self);
gboolean cfg_init(GlobalConfig *cfg);
gboolean cfg_deinit(GlobalConfig *cfg);

PersistConfig *persist_config_new(void);
void persist_config_free(PersistConfig *self);
void cfg_persist_config_move(GlobalConfig *src, GlobalConfig *dest);
void cfg_persist_config_add(GlobalConfig *cfg, const gchar *name, gpointer value, GDestroyNotify destroy,
                            gboolean force);
gpointer cfg_persist_config_fetch(GlobalConfig *cfg, const gchar *name);

typedef gboolean(* mangle_callback)(GlobalConfig *cfg, LogMessage *msg, gpointer user_data);

void register_source_mangle_callback(GlobalConfig *src,mangle_callback cb);
void uregister_source_mangle_callback(GlobalConfig *src,mangle_callback cb);

static inline gboolean
cfg_is_config_version_older(GlobalConfig *cfg, gint req)
{
  if (!cfg)
    return FALSE;
  if (version_convert_from_user(cfg->user_version) >= req)
    return FALSE;
  return TRUE;
}

static inline void
cfg_set_use_uniqid(gboolean flag)
{
  configuration->use_uniqid = !!flag;
}

gint cfg_get_user_version(const GlobalConfig *cfg);
guint cfg_get_parsed_version(const GlobalConfig *cfg);
const gchar *cfg_get_filename(const GlobalConfig *cfg);

#endif
