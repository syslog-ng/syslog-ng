/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
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
typedef struct _PersistentConfig PersistentConfig;

/* configuration data as loaded from the config file */
struct _GlobalConfig
{
  /* version number of the configuration file, hex-encoded syslog-ng major/minor, e.g. 0x0201 is syslog-ng 2.1 format */
  gint version;
  gchar *filename;

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
  uid_t file_uid;
  gid_t file_gid;
  mode_t file_perm;
  
  uid_t dir_uid;
  gid_t dir_gid;
  mode_t dir_perm;

  gboolean keep_timestamp;  
  gint ts_format;
  gint frac_digits;

  gchar *recv_time_zone_string;
  gchar *send_time_zone_string;
  
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
  PersistentConfig *persist;

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

void cfg_file_owner_set(GlobalConfig *self, gchar *owner);
void cfg_file_group_set(GlobalConfig *self, gchar *group);
void cfg_file_perm_set(GlobalConfig *self, gint perm);
void cfg_bad_hostname_set(GlobalConfig *self, gchar *bad_hostname_re);

void cfg_dir_owner_set(GlobalConfig *self, gchar *owner);
void cfg_dir_group_set(GlobalConfig *self, gchar *group);
void cfg_dir_perm_set(GlobalConfig *self, gint perm);
gint cfg_tz_convert_value(gchar *convert);
gint cfg_ts_format_value(gchar *format);

GlobalConfig *cfg_new(gchar *fname);
void cfg_free(GlobalConfig *self);
gboolean cfg_init(GlobalConfig *cfg);
gboolean cfg_deinit(GlobalConfig *cfg);
GlobalConfig *cfg_reload_config(gchar *fname, GlobalConfig *cfg);

PersistentConfig *persist_config_new(void);
void cfg_persist_config_add(GlobalConfig *cfg, gchar *name, gpointer value, gssize value_len, GDestroyNotify destroy, gboolean force);
void cfg_persist_config_add_survivor(GlobalConfig *cfg, gchar *name, gchar *value, gssize value_len, gboolean force);
gpointer cfg_persist_config_fetch(GlobalConfig *cfg, gchar *name, gsize *result_len, gint *version);
void cfg_persist_config_save(GlobalConfig *cfg, const gchar *filename);
void cfg_persist_config_load(GlobalConfig *cfg, const gchar *filename);
gint cfg_persist_get_version(GlobalConfig *cfg);
void cfg_persist_set_version(GlobalConfig *cfg, const gint version);

void persist_config_free(PersistentConfig *persist);

/* defined in the lexer */
void yyerror(char *msg);
int yylex();
int cfg_lex_init(FILE *file, gint init_line_num);
void cfg_lex_deinit(void);
gboolean cfg_lex_process_include(const gchar *filename);
const gchar *cfg_lex_get_current_file(void);
gint cfg_lex_get_current_lineno(void);

/* defined in the parser */
int yyparse(void);

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
