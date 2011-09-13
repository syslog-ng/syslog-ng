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
  
#include "cfg.h"
#include "sgroup.h"
#include "dgroup.h"
#include "filter.h"
#include "center.h"
#include "messages.h"
#include "templates.h"
#include "misc.h"
#include "logmsg.h"
#include "dnscache.h"
#include "logparser.h"
#include "serialize.h"
#include "plugin.h"
#include "cfg-parser.h"
#include "stats.h"

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iv_work.h>

/* PersistentConfig */

struct _PersistConfig
{
  GHashTable *keys;
};

typedef struct _PersistConfigEntry
{
  gpointer value;
  GDestroyNotify destroy;
} PersistConfigEntry;

static void
persist_config_entry_free(PersistConfigEntry *self)
{
  if (self->destroy)
    {
      self->destroy(self->value);
    }
  g_free(self);
}

PersistConfig *
persist_config_new(void)
{
  PersistConfig *self = g_new0(PersistConfig, 1);

  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) persist_config_entry_free);
  return self;
}

void
persist_config_free(PersistConfig *self)
{
  g_hash_table_destroy(self->keys);
  g_free(self);
}

gint
cfg_ts_format_value(gchar *format)
{
  if (strcmp(format, "rfc3164") == 0 || strcmp(format, "bsd") == 0)
    return TS_FMT_BSD;
  else if (strcmp(format, "rfc3339") == 0 || strcmp(format, "iso") == 0)
    return TS_FMT_ISO;
  else if (strcmp(format, "full") == 0)
    return TS_FMT_FULL;
  else if (strcmp(format, "unix") == 0 || strcmp(format, "utc") == 0)
    return TS_FMT_UNIX;
  else
    {
      msg_error("Invalid ts_format() value",
                evt_tag_str("value", format),
                NULL);
      return TS_FMT_BSD;
    }
}

void
cfg_file_owner_set(GlobalConfig *self, gchar *owner)
{
  if (!resolve_user(owner, &self->file_uid))
    msg_error("Error resolving user",
               evt_tag_str("user", owner),
               NULL);
}

void
cfg_file_group_set(GlobalConfig *self, gchar *group)
{
  if (!resolve_group(group, &self->file_gid))
    msg_error("Error resolving group",
               evt_tag_str("group", group),
               NULL);
}

void
cfg_file_perm_set(GlobalConfig *self, gint perm)
{
  self->file_perm = perm;
}

void
cfg_dir_owner_set(GlobalConfig *self, gchar *owner)
{
  if (!resolve_user(owner, &self->dir_uid))
    msg_error("Error resolving user",
               evt_tag_str("user", owner),
               NULL);
}

void
cfg_dir_group_set(GlobalConfig *self, gchar *group)
{
  if (!resolve_group(group, &self->dir_gid))
    msg_error("Error resolving group",
               evt_tag_str("group", group),
               NULL);
}

void
cfg_dir_perm_set(GlobalConfig *self, gint perm)
{
  self->dir_perm = perm;
}

void
cfg_bad_hostname_set(GlobalConfig *self, gchar *bad_hostname_re)
{
  if (self->bad_hostname_re)
    g_free(self->bad_hostname_re);
  self->bad_hostname_re = g_strdup(bad_hostname_re);  
}

void
cfg_add_source(GlobalConfig *cfg, LogSourceGroup *group)
{
  g_hash_table_insert(cfg->sources, group->name, group);
}

void
cfg_add_dest(GlobalConfig *cfg, LogDestGroup *group)
{
  g_hash_table_insert(cfg->destinations, group->name, group);
}

void
cfg_add_filter(GlobalConfig *cfg, LogProcessRule *rule)
{
  g_hash_table_insert(cfg->filters, rule->name, rule);
}

void
cfg_add_parser(GlobalConfig *cfg, LogProcessRule *rule)
{
  g_hash_table_insert(cfg->parsers, rule->name, rule);
}

void
cfg_add_rewrite(GlobalConfig *cfg, LogProcessRule *rule)
{
  g_hash_table_insert(cfg->rewriters, rule->name, rule);
}

void
cfg_add_connection(GlobalConfig *cfg, LogConnection *conn)
{
  g_ptr_array_add(cfg->connections, conn);
}

void
cfg_add_template(GlobalConfig *cfg, LogTemplate *template)
{
  g_hash_table_insert(cfg->templates, template->name, template);
}

LogTemplate *
cfg_lookup_template(GlobalConfig *cfg, const gchar *name)
{
  if (name)
    return log_template_ref(g_hash_table_lookup(cfg->templates, name));
  return NULL;
}

extern GTimeVal app_uptime;
gboolean
cfg_init(GlobalConfig *cfg)
{
  gint regerr;

  /* init the uptime (SYSUPTIME macro) */
  g_get_current_time(&app_uptime);
  
  if (cfg->file_template_name && !(cfg->file_template = cfg_lookup_template(cfg, cfg->file_template_name)))
    msg_error("Error resolving file template",
               evt_tag_str("name", cfg->file_template_name),
               NULL);
  if (cfg->proto_template_name && !(cfg->proto_template = cfg_lookup_template(cfg, cfg->proto_template_name)))
    msg_error("Error resolving protocol template",
               evt_tag_str("name", cfg->proto_template_name),
               NULL);
  stats_reinit(cfg);

  if (cfg->bad_hostname_re)
    {
      if ((regerr = regcomp(&cfg->bad_hostname, cfg->bad_hostname_re, REG_NOSUB | REG_EXTENDED)) != 0)
        {
          gchar buf[256];
          
          regerror(regerr, &cfg->bad_hostname, buf, sizeof(buf));
          msg_error("Error compiling bad_hostname regexp",
                    evt_tag_str("error", buf),
                    NULL);
        }
      else
        { 
          cfg->bad_hostname_compiled = TRUE;
        }
    }
  dns_cache_set_params(cfg->dns_cache_size, cfg->dns_cache_expire, cfg->dns_cache_expire_failed, cfg->dns_cache_hosts);
  return log_center_init(cfg->center, cfg);
}

gboolean
cfg_deinit(GlobalConfig *cfg)
{
  return log_center_deinit(cfg->center);
}

void
cfg_set_version(GlobalConfig *self, gint version)
{
  self->version = version;
  if (self->version < CFG_CURRENT_VERSION)
    {
      msg_warning("WARNING: Configuration file format is too old, please update it to use the " CFG_CURRENT_VERSION_STRING " format as some constructs might operate inefficiently",
                  NULL);
    }
  else if (self->version > CFG_CURRENT_VERSION)
    {
      msg_warning("WARNING: Configuration file format is newer than the current version, please specify the current version number (" CFG_CURRENT_VERSION_STRING ") in the @version directive",
                  NULL);
      self->version = CFG_CURRENT_VERSION;
    }

  if (self->version < 0x0300)
    {
      msg_warning("WARNING: global: the default value of chain_hostnames is changing to 'no' in version 3.0, please update your configuration accordingly",
                  NULL);
      self->chain_hostnames = TRUE;
    }

  if (self->version < 0x0303)
    {
      msg_warning("WARNING: global: the default value of log_fifo_size() has changed to 10000 in version 3.3 to reflect log_iw_size() changes for tcp()/udp() window size changes",
                  NULL);
    }

  if (self->version <= 0x0301 || atoi(cfg_args_get(self->lexer->globals, "autoload-compiled-modules")))
    {
      gint i;
      gchar **mods;

      mods = g_strsplit(default_modules, ",", 0);
      for (i = 0; mods[i]; i++)
        {
          plugin_load_module(mods[i], self, NULL);
        }
      g_strfreev(mods);
    }
}

struct _LogTemplate *
cfg_check_inline_template(GlobalConfig *cfg, const gchar *template_or_name, GError **error)
{
  LogTemplate *template = cfg_lookup_template(configuration, template_or_name);

  if (template == NULL)
    {
      template = log_template_new(cfg, NULL);
      log_template_compile(template, template_or_name, error);
      template->def_inline = TRUE;
    }
  return template;
}

GlobalConfig *
cfg_new(gint version)
{
  GlobalConfig *self = g_new0(GlobalConfig, 1);

  self->version = version;
  self->sources = g_hash_table_new(g_str_hash, g_str_equal);
  self->destinations = g_hash_table_new(g_str_hash, g_str_equal);
  self->filters = g_hash_table_new(g_str_hash, g_str_equal);
  self->parsers = g_hash_table_new(g_str_hash, g_str_equal);
  self->rewriters = g_hash_table_new(g_str_hash, g_str_equal);
  self->templates = g_hash_table_new(g_str_hash, g_str_equal);
  self->connections = g_ptr_array_new();

  self->flush_lines = 0;
  self->flush_timeout = 10000;  /* 10 seconds */
  self->mark_freq = 1200;	/* 20 minutes */
  self->stats_freq = 600;
  self->chain_hostnames = 0;
  self->use_fqdn = 0;
  self->use_dns = 1;
  self->time_reopen = 60;
  self->time_reap = 60;

  self->log_fifo_size = 10000;
  self->log_msg_size = 8192;

  self->follow_freq = -1;
  self->file_uid = 0;
  self->file_gid = 0;
  self->file_perm = 0600;
  self->dir_uid = 0;
  self->dir_gid = 0;
  self->dir_perm = 0700;

  self->use_dns_cache = 1;
  self->dns_cache_size = 1007;
  self->dns_cache_expire = 3600;
  self->dns_cache_expire_failed = 60;
  self->threaded = FALSE;
  
  log_template_options_defaults(&self->template_options);
  self->template_options.ts_format = TS_FMT_BSD;
  self->template_options.frac_digits = 0;
  self->recv_time_zone = NULL;
  self->keep_timestamp = TRUE;
  self->cfg_fingerprint = NULL;
  self->stats_reset = FALSE;
  return self;
}

gboolean
cfg_run_parser(GlobalConfig *self, CfgLexer *lexer, CfgParser *parser, gpointer *result, gpointer arg)
{
  gboolean res;
  GlobalConfig *old_cfg;
  CfgLexer *old_lexer;

  old_cfg = configuration;
  configuration = self;
  old_lexer = self->lexer;
  self->lexer = lexer;
  cfg_args_set(self->lexer->globals, "syslog-ng-root", PATH_PREFIX);
  cfg_args_set(self->lexer->globals, "syslog-ng-data", PATH_DATADIR);
  cfg_args_set(self->lexer->globals, "module-path", module_path);
  cfg_args_set(self->lexer->globals, "include-path", PATH_SYSCONFDIR);
  cfg_args_set(self->lexer->globals, "autoload-compiled-modules", "1");

  res = cfg_parser_parse(parser, lexer, result, arg);

  cfg_lexer_free(lexer);
  self->lexer = NULL;
  self->lexer = old_lexer;
  configuration = old_cfg;
  return res;
}

gboolean
cfg_read_config(GlobalConfig *self, gchar *fname, gboolean syntax_only, gchar *preprocess_into)
{
  FILE *cfg_file;
  gint res;

  self->filename = fname;

  if ((cfg_file = fopen(fname, "r")) != NULL)
    {
      CfgLexer *lexer;

      lexer = cfg_lexer_new(cfg_file, fname, preprocess_into);
      res = cfg_run_parser(self, lexer, &main_parser, (gpointer *) &self, NULL);
      fclose(cfg_file);
      if (res)
	{
	  /* successfully parsed */
	  self->center = log_center_new();
	  return TRUE;
	}
    }
  else
    {
      msg_error("Error opening configuration file",
                evt_tag_str(EVT_TAG_FILENAME, fname),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
    }
  
  return FALSE;
}

static gboolean
cfg_remove_pipe(gpointer key, gpointer value, gpointer user_data)
{
  LogPipe *self = (LogPipe *) value;
  
  log_pipe_unref(self);
  return TRUE;
}

static gboolean
cfg_remove_process(gpointer key, gpointer value, gpointer user_data)
{
  LogProcessRule *s = (LogProcessRule *) value;
  
  log_process_rule_unref(s);
  return TRUE;
}

static gboolean
cfg_remove_template(gpointer key, gpointer value, gpointer user_data)
{
  log_template_unref(value);
  return TRUE;
}

void
cfg_free(GlobalConfig *self)
{
  int i;

  g_assert(self->persist == NULL);
  if (self->state)
    persist_state_free(self->state);

  g_free(self->file_template_name);
  g_free(self->proto_template_name);  
  log_template_unref(self->file_template);
  log_template_unref(self->proto_template);
  
  if (self->center)
    log_center_free(self->center);
  
  g_hash_table_foreach_remove(self->sources, cfg_remove_pipe, NULL);
  g_hash_table_foreach_remove(self->destinations, cfg_remove_pipe, NULL);
  g_hash_table_foreach_remove(self->filters, cfg_remove_process, NULL);
  g_hash_table_foreach_remove(self->parsers, cfg_remove_process, NULL);
  g_hash_table_foreach_remove(self->rewriters, cfg_remove_process, NULL);
  g_hash_table_foreach_remove(self->templates, cfg_remove_template, NULL);
  
  for (i = 0; i < self->connections->len; i++)
    {
      log_connection_free(g_ptr_array_index(self->connections, i));
    }
  g_hash_table_destroy(self->sources);
  g_hash_table_destroy(self->destinations);
  g_hash_table_destroy(self->filters);
  g_hash_table_destroy(self->parsers);
  g_hash_table_destroy(self->rewriters);
  g_hash_table_destroy(self->templates);
  g_ptr_array_free(self->connections, TRUE);
  if (self->bad_hostname_compiled)
    regfree(&self->bad_hostname);
  g_free(self->bad_hostname_re);
  g_free(self->dns_cache_hosts);
  g_list_free(self->plugins);
  g_free(self);
}

void
cfg_persist_config_move(GlobalConfig *src, GlobalConfig *dest)
{
  if (dest->persist != NULL)
    persist_config_free(dest->persist);
  dest->persist = src->persist;
  dest->state = src->state;
  src->persist = NULL;
  src->state = NULL;
}

void
cfg_persist_config_add(GlobalConfig *cfg, gchar *name, gpointer value, GDestroyNotify destroy, gboolean force)
{ 
  PersistConfigEntry *p;
  
  if (cfg->persist && value)
    {
      if (g_hash_table_lookup(cfg->persist->keys, name))
        {
          if (!force)
            {
              msg_error("Internal error, duplicate configuration elements refer to the same persistent config", 
                        evt_tag_str("name", name),
                        NULL);
              destroy(value);
              return;
            }
        }
  
      p = g_new0(PersistConfigEntry, 1);
  
      p->value = value;
      p->destroy = destroy;
      g_hash_table_insert(cfg->persist->keys, g_strdup(name), p);
      return;
    }
  else if (destroy && value)
    {
      destroy(value);
    }
  return;
}

gpointer
cfg_persist_config_fetch(GlobalConfig *cfg, gchar *name)
{
  gpointer res = NULL;
  gchar *orig_key;
  PersistConfigEntry *p;
  gpointer tmp1, tmp2;

  if (cfg->persist && g_hash_table_lookup_extended(cfg->persist->keys, name, &tmp1, &tmp2))
    {
      orig_key = (gchar *) tmp1;
      p = (PersistConfigEntry *) tmp2;

      res = p->value;

      g_hash_table_steal(cfg->persist->keys, name);
      g_free(orig_key);
      g_free(p);
    }
  return res;
}

