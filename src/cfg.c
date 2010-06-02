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

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

gboolean
cfg_init(GlobalConfig *cfg)
{
  gint regerr;
  
  if (cfg->file_template_name && !(cfg->file_template = cfg_lookup_template(cfg, cfg->file_template_name)))
    msg_error("Error resolving file template",
               evt_tag_str("name", cfg->file_template_name),
               NULL);
  if (cfg->proto_template_name && !(cfg->proto_template = cfg_lookup_template(cfg, cfg->proto_template_name)))
    msg_error("Error resolving protocol template",
               evt_tag_str("name", cfg->proto_template_name),
               NULL);

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
  if (self->version < 0x0300)
    {
      msg_warning("Configuration file format is not current, please update it to use the 3.0 format as some constructs might operate inefficiently",
                  NULL);
    }

  if (self->version < 0x0300)
    {
      msg_warning("WARNING: global: the default value of chain_hostnames is changing to 'no' in version 3.0, please update your configuration accordingly",
                  NULL);
      self->chain_hostnames = TRUE;
    }

  if (self->version <= 0x0301 || atoi(cfg_args_get(self->lexer->globals, "autoload-compiled-modules")))
    {
      /* auto load modules for old configurations */
      plugin_load_module("afsocket", self, NULL);
#if ENABLE_SQL_MODULE
      plugin_load_module("afsql", self, NULL);
#endif
#if ENABLE_SUN_STREAMS_MODULE
      plugin_load_module("afstreams", self, NULL);
#endif
    }
}

struct _LogTemplate *
cfg_check_inline_template(GlobalConfig *cfg, const gchar *template_or_name)
{
  struct _LogTemplate *template = cfg_lookup_template(configuration, template_or_name);
  if (template == NULL)
    {
      template = log_template_new(NULL, template_or_name);
      template->def_inline = TRUE;
    }
  return template;
}

GlobalConfig *
cfg_new(gchar *fname)
{
  GlobalConfig *self = g_new0(GlobalConfig, 1);
  FILE *cfg_file;
  gint res;

  self->filename = fname;
  
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

  self->log_fifo_size = 1000;
  self->log_fetch_limit = 10;
  self->log_iw_size = 100;
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
  
  self->ts_format = TS_FMT_BSD;
  self->frac_digits = 0;
  self->recv_time_zone = NULL;
  self->send_time_zone = NULL;
  self->local_time_zone = NULL;
  self->keep_timestamp = TRUE;
  
  self->persist = persist_config_new();
 
  configuration = self;
 
  if ((cfg_file = fopen(fname, "r")) != NULL)
    {
      self->lexer = cfg_lexer_new(cfg_file, fname);
      cfg_args_set(self->lexer->globals, "syslog-ng-root", PATH_PREFIX);
      cfg_args_set(self->lexer->globals, "module-path", PATH_PLUGINDIR);
      cfg_args_set(self->lexer->globals, "include-path", PATH_SYSCONFDIR);
      cfg_args_set(self->lexer->globals, "autoload-compiled-modules", "1");
      res = cfg_parser_parse(&main_parser, self->lexer, (gpointer *) &self);
      cfg_lexer_free(self->lexer);
      self->lexer = NULL;

      fclose(cfg_file);
      if (res)
	{
	  /* successfully parsed */
	  self->center = log_center_new();
	  return self;
	}
    }
  else
    {
      msg_error("Error opening configuration file",
                evt_tag_str(EVT_TAG_FILENAME, fname),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
    }
  
  cfg_free(self);
  configuration = NULL;
  return NULL;
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

  if (self->persist != NULL)
    persist_config_free(self->persist);

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
  g_free(self);
  configuration = NULL;
}

static void 
cfg_persist_config_move(GlobalConfig *src, GlobalConfig *dest)
{
  if (dest->persist != NULL)
    persist_config_free(dest->persist);
  dest->persist = src->persist;
  src->persist = NULL;
}

GlobalConfig *
cfg_reload_config(gchar *fname, GlobalConfig *cfg)
{
  GlobalConfig *new_cfg;
  new_cfg = cfg_new(fname);
  if (!new_cfg)
    {
      msg_error("Error parsing configuration",
                evt_tag_str(EVT_TAG_FILENAME, fname),
                NULL);
      return cfg;
    }
  
  cfg_deinit(cfg);
  cfg_persist_config_move(cfg, new_cfg);

  if (cfg_init(new_cfg))
    {
      msg_verbose("New configuration initialized", NULL);
      cfg_free(cfg);
      return new_cfg;
    }
  else
    {
      msg_error("Error initializing new configuration, reverting to old config", NULL);
      cfg_persist_config_move(new_cfg, cfg);
      if (!cfg_init(cfg))
        {
          /* hmm. hmmm, error reinitializing old configuration, we're hosed.
           * Best is to kill ourselves in the hope that the supervisor
           * restarts us.
           */
          kill(getpid(), SIGQUIT);
          g_assert_not_reached();
        }
      return cfg;
    }
}

/* PersistentConfig */

struct _PersistentConfig
{
  gint version;
  GHashTable *keys;
};

typedef struct _PersistentConfigEntry
{
  gpointer value;
  gssize value_len;
  GDestroyNotify destroy;
  gint version;
  /* this value is a string and should be saved */
  gboolean survive_across_restarts:1;
} PersistentConfigEntry;

static void
cfg_persist_config_entry_free(PersistentConfigEntry *self)
{
  if (self->destroy)
  {
    self->destroy(self->value);
    self->value_len = 0;
  }
  g_free(self);
}

static PersistentConfigEntry *
cfg_persist_config_add_entry(GlobalConfig *cfg, gchar *name, gpointer value, gssize value_len, GDestroyNotify destroy, gint version, gboolean force)
{ 
  PersistentConfigEntry *p;
  
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
              return NULL;
            }
        }
  
      p = g_new0(PersistentConfigEntry, 1);
  
      p->value = value;
      p->destroy = destroy;
      p->value_len = value_len;
      p->version = version < 0 ? cfg->persist->version : version;
      g_hash_table_insert(cfg->persist->keys, g_strdup(name), p);
      return p;
    }
  return NULL;
}

void
cfg_persist_config_add(GlobalConfig *cfg, gchar *name, gpointer value, gssize value_len, GDestroyNotify destroy, gboolean force)
{ 
  cfg_persist_config_add_entry(cfg, name, value, value_len, destroy, -1, force);
}

/**
 * cfg_persist_config_add_survivor:
 *
 * Add a "surviving" value, a value that is remembered accross restarts.
 **/
void
cfg_persist_config_add_survivor(GlobalConfig *cfg, gchar *name, gchar *value, gssize value_len, gboolean force)
{
  PersistentConfigEntry *p;
  
  if (value_len < 0)
    p = cfg_persist_config_add_entry(cfg, name, g_strdup(value), -1, g_free, -1, force);
  else
    p = cfg_persist_config_add_entry(cfg, name, g_memdup(value, value_len), value_len, g_free, -1, force);

  if (p)
    {
      p->survive_across_restarts = TRUE;
    }
}

gpointer
cfg_persist_config_fetch(GlobalConfig *cfg, gchar *name, gsize *result_len, gint *version)
{
  gpointer res = NULL;
  gchar *orig_key;
  PersistentConfigEntry *p;
  gpointer tmp1, tmp2;

  if (cfg->persist && g_hash_table_lookup_extended(cfg->persist->keys, name, &tmp1, &tmp2))
    {
      orig_key = (gchar *) tmp1;
      p = (PersistentConfigEntry *) tmp2;

      res = p->value;

      if (result_len)
        *result_len = p->value_len;
      if (version)
        *version = p->version;

      g_hash_table_steal(cfg->persist->keys, name);
      g_free(orig_key);
      g_free(p);
    }
  return res;
}

static void
cfg_persist_config_save_value(gchar *key, PersistentConfigEntry *entry, SerializeArchive *sa)
{
  if (entry->survive_across_restarts)
    {
      /* NOTE: we ignore errors here, as we cannot bail out from the
       * g_hash_table_foreach() loop anyway. */
      
      serialize_write_cstring(sa, key, -1);
      serialize_write_cstring(sa, (gchar *) entry->value, entry->value_len);
    }
}

void
cfg_persist_config_save(GlobalConfig *cfg, const gchar *filename)
{
  FILE *persist_file;
  
  persist_file = fopen(filename, "w");
  if (persist_file)
    {
      SerializeArchive *sa;
      
      sa = serialize_file_archive_new(persist_file);
      
      serialize_write_blob(sa, "SLP3", 4);
      g_hash_table_foreach(cfg->persist->keys, (GHFunc) cfg_persist_config_save_value, sa);
      serialize_write_cstring(sa, "", 0);
      if (sa->error)
        goto error;
        
      serialize_archive_free(sa);
      fclose(persist_file);
      return;
    }
 error:
  msg_error("Error saving persistent configuration file",
            evt_tag_str("name", PATH_PERSIST_CONFIG),
            NULL);
  
}

void
cfg_persist_config_load(GlobalConfig *cfg, const gchar *filename)
{
  FILE *persist_file;
  
  persist_file = fopen(filename, "r");
  if (persist_file)
    {
      gchar magic[4];
      gchar *key, *value;
      SerializeArchive *sa;
      gint version;
      
      sa = serialize_file_archive_new(persist_file);
      serialize_read_blob(sa, magic, 4);
      if (memcmp(magic, "SLP2", 4) != 0 && memcmp(magic, "SLP3", 4) != 0)
        {
          msg_error("Persistent configuration file is in invalid format, ignoring", NULL);
          goto close_and_exit;
        }
      version = magic[3] - '0';

      while (serialize_read_cstring(sa, &key, NULL))
        {
          gsize len;
          if (key[0] && serialize_read_cstring(sa, &value, &len))
            {
              /* add a non-surviving entry, thus each value is only
               * written/read once unless the code readds it, this is needed
               * to limit the size of the persistent configuration file */
              
              cfg_persist_config_add_entry(cfg, key, value, len, g_free, version, FALSE);
            }
          else
            {
              g_free(key);
              break;
            }
          g_free(key);
        }
 close_and_exit:
      serialize_archive_free(sa);
      fclose(persist_file);
    }
}

PersistentConfig *
persist_config_new(void)
{
  PersistentConfig *self = g_new0(PersistentConfig, 1);
  self->version = 3;
  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) cfg_persist_config_entry_free);
  return self;
}

void 
persist_config_free(PersistentConfig *self)
{
  g_hash_table_destroy(self->keys);
  g_free(self);
}

