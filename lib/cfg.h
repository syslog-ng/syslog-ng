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
#include "cfg-persist.h"
#include "plugin.h"
#include "persist-state.h"
#include "template/templates.h"
#include "host-resolve.h"
#include "logmsg/type-hinting.h"
#include "stats/stats.h"
#include "healthcheck/healthcheck-stats.h"
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

/* configuration data as loaded from the config file */
struct _GlobalConfig
{
  /* version number specified by the user, set _after_ parsing is complete */
  /* hex-encoded syslog-ng major/minor, e.g. 0x0201 is syslog-ng 2.1 format */
  gint user_version;

  /* config identifier specified by the user */
  gchar *user_config_id;

  guint8 *config_hash;

  const gchar *filename;
  PluginContext plugin_context;
  gboolean use_plugin_discovery;
  gboolean enable_forced_modules;
  CfgLexer *lexer;
  CfgArgs *globals;

  StatsOptions stats_options;
  HealthCheckStatsOptions healthcheck_options;
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
  gint log_level;

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

  GList *file_list;
};

gboolean cfg_load_module_with_args(GlobalConfig *cfg, const gchar *module_name, CfgArgs *args);
gboolean cfg_load_module(GlobalConfig *cfg, const gchar *module_name);
gboolean cfg_is_module_available(GlobalConfig *self, const gchar *module_name);
void cfg_discover_candidate_modules(GlobalConfig *self);

Plugin *cfg_find_plugin(GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
gpointer cfg_parse_plugin(GlobalConfig *cfg, Plugin *plugin, CFG_LTYPE *yylloc, gpointer arg);

gboolean cfg_allow_config_dups(GlobalConfig *self);

void cfg_bad_hostname_set(GlobalConfig *self, gchar *bad_hostname_re);
gint cfg_lookup_mark_mode(const gchar *mark_mode);
void cfg_set_mark_mode(GlobalConfig *self, const gchar *mark_mode);
gboolean cfg_set_log_level(GlobalConfig *self, const gchar *log_level);

gint cfg_tz_convert_value(gchar *convert);
gint cfg_ts_format_value(gchar *format);

void cfg_set_version_without_validation(GlobalConfig *self, gint version);
gboolean cfg_set_version(GlobalConfig *self, gint version);
gboolean cfg_set_current_version(GlobalConfig *self);
void cfg_set_user_config_id(GlobalConfig *self, const gchar *id);

void cfg_format_id(GlobalConfig *self, GString *id);

void cfg_set_global_paths(GlobalConfig *self);

GlobalConfig *cfg_new(gint version);
GlobalConfig *cfg_new_snippet(void);
GlobalConfig *cfg_new_subordinate(GlobalConfig *master);
gboolean cfg_run_parser(GlobalConfig *self, CfgLexer *lexer, CfgParser *parser, gpointer *result, gpointer arg);
gboolean cfg_run_parser_with_main_context(GlobalConfig *self, CfgLexer *lexer, CfgParser *parser, gpointer *result,
                                          gpointer arg, const gchar *desc);
gboolean cfg_read_config(GlobalConfig *cfg, const gchar *fname, gchar *preprocess_into);
void cfg_load_forced_modules(GlobalConfig *self);
void cfg_shutdown(GlobalConfig *self);
gboolean cfg_is_shutting_down(GlobalConfig *cfg);
void cfg_free(GlobalConfig *self);
gboolean cfg_init(GlobalConfig *cfg);
gboolean cfg_deinit(GlobalConfig *cfg);

PersistConfig *persist_config_new(void);
void persist_config_free(PersistConfig *self);
void cfg_persist_config_move(GlobalConfig *src, GlobalConfig *dest);
void cfg_persist_config_add(GlobalConfig *cfg, const gchar *name, gpointer value, GDestroyNotify destroy);
gpointer cfg_persist_config_fetch(GlobalConfig *cfg, const gchar *name);

typedef gboolean(* mangle_callback)(GlobalConfig *cfg, LogMessage *msg, gpointer user_data);

void register_source_mangle_callback(GlobalConfig *src, mangle_callback cb);
gboolean is_source_mangle_callback_registered(GlobalConfig *src, mangle_callback cb);
void uregister_source_mangle_callback(GlobalConfig *src, mangle_callback cb);

static inline gboolean
__cfg_is_config_version_older(GlobalConfig *cfg, gint req)
{
  if (!cfg)
    return FALSE;
  if (version_convert_from_user(cfg->user_version) >= req)
    return FALSE;
  return TRUE;
}

/* VERSION_VALUE_LAST_SEMANTIC_CHANGE needs to be bumped in versioning.h whenever
 * we add a conditional on the config version anywhere in the codebase.  The
 * G_STATIC_ASSERT checks that we indeed did that.
 *
 * We also allow merging the features for the upcoming major version.
 */

#define cfg_is_config_version_older(__cfg, __req) \
  ({ \
    /* check that VERSION_VALUE_LAST_SEMANTIC_CHANGE is set correctly */ G_STATIC_ASSERT((__req) <= VERSION_VALUE_LAST_SEMANTIC_CHANGE || (__req) == VERSION_VALUE_NEXT_MAJOR); \
    __cfg_is_config_version_older(__cfg, __req); \
  })

/* This function returns TRUE if a version based feature flip is enabled.
 * This will enable or disable feature related upgrade warnings.
 *
 * This was initially introduced for "typing" support, where the following
 * is implemented:
 *
 * As long as we are using a 3.x version number (less than 3.255), the upgrade
 * warnings are suppressed and compatibility mode is enabled.  Once @version
 * is something equal or larger than 3.255, we still enable compatibility but
 * warnings will NOT be suppressed.  This is controlled with
 * FEATURE_TYPING_MIN_VERSION.
 *
 * Once we release 4.0, FEATURE_TYPING_MIN_VERSION needs to be set to zero in
 * versioning.h With that all upgrade notices start to appear when syslog-ng
 * finds a 3.x configuration.
 */
#define __cfg_is_feature_enabled(cfg, min_version) \
  ({ \
    /* the flip-over for min_version reached, set min_version to 0 */ G_STATIC_ASSERT(min_version == 0 || VERSION_VALUE_CURRENT < ((min_version) - 1));       \
    version_convert_from_user(cfg->user_version) >= (min_version) - 1;  \
  })

#define cfg_is_feature_enabled(cfg, topic)        __cfg_is_feature_enabled(cfg, FEATURE_ ## topic ## _MIN_VERSION)
#define cfg_is_typing_feature_enabled(cfg)        cfg_is_feature_enabled(cfg, TYPING)

#define cfg_is_experimental_feature_enabled(cfg)  0

static inline void
cfg_set_use_uniqid(gboolean flag)
{
  configuration->use_uniqid = !!flag;
}

gint cfg_get_user_version(const GlobalConfig *cfg);
guint cfg_get_parsed_version(const GlobalConfig *cfg);
const gchar *cfg_get_filename(const GlobalConfig *cfg);

static inline EVTTAG *
cfg_format_version_tag(const gchar *tag_name, gint version)
{
  return evt_tag_printf(tag_name, "%d.%d", (version & 0xFF00) >> 8, version & 0xFF);
}

static inline EVTTAG *
cfg_format_config_version_tag(GlobalConfig *self)
{
  return cfg_format_version_tag("config-version", self->user_version);
}


#endif
