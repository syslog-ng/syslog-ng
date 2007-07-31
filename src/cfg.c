/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

GlobalConfig *configuration;

gboolean
cfg_timezone_value(gchar *tz, glong *timezone)
{
  *timezone = -1;
  if ((*tz == '+' || *tz == '-') && strlen(tz) == 6 && 
      isdigit((int) *(tz+1)) && isdigit((int) *(tz+2)) && (*(tz+3) == ':') && isdigit((int) *(tz+4)) && isdigit((int) *(tz+5)))
    {
      /* timezone offset */
      gint sign = *tz == '-' ? -1 : 1;
      gint hours, mins;
      tz++;
      
      hours = (*tz - '0') * 10 + *(tz+1) - '0';
      mins = (*(tz+3) - '0') * 10 + *(tz+4) - '0';
      if ((hours < 24 && mins <= 60) || (hours == 24 && mins == 0))
        {
          *timezone = sign * (hours * 3600 + mins * 60);
          return TRUE;
        }
    }
  msg_error("Bogus timezone spec, must be in the format [+-]HH:MM, offset must be less than 24:00",
            evt_tag_str("value", tz),
            NULL);
  return FALSE;
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
  g_hash_table_insert(cfg->sources, group->name->str, group);
}

void
cfg_add_dest(GlobalConfig *cfg, LogDestGroup *group)
{
  g_hash_table_insert(cfg->destinations, group->name->str, group);
}

void
cfg_add_filter(GlobalConfig *cfg, LogFilterRule *rule)
{
  g_hash_table_insert(cfg->filters, rule->name->str, rule);
}

void
cfg_add_connection(GlobalConfig *cfg, LogConnection *conn)
{
  g_ptr_array_add(cfg->connections, conn);
}

void
cfg_add_template(GlobalConfig *cfg, LogTemplate *template)
{
  g_hash_table_insert(cfg->templates, template->name->str, template);
}

LogTemplate *
cfg_lookup_template(GlobalConfig *cfg, gchar *name)
{
  if (name)
    return log_template_ref(g_hash_table_lookup(cfg->templates, name));
  return NULL;
}

gboolean
cfg_init(GlobalConfig *cfg, PersistentConfig *persist)
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
  return cfg->center->super.init(&cfg->center->super, cfg, persist);
}

gboolean
cfg_deinit(GlobalConfig *cfg, PersistentConfig *persist)
{
  return cfg->center->super.deinit(&cfg->center->super, cfg, persist);
}

/* extern declarations in the generated parser & lexer */
extern FILE *yyin;
extern int yyparse();
extern void lex_init(FILE *);
extern int yydebug;
extern int linenum;

extern void yyparser_reset(void);

GlobalConfig *
cfg_new(gchar *fname)
{
  GlobalConfig *self = g_new0(GlobalConfig, 1);
  FILE *cfg;
  gint res;

  self->filename = fname;

  self->sources = g_hash_table_new(g_str_hash, g_str_equal);
  self->destinations = g_hash_table_new(g_str_hash, g_str_equal);
  self->filters = g_hash_table_new(g_str_hash, g_str_equal);
  self->templates = g_hash_table_new(g_str_hash, g_str_equal);
  self->connections = g_ptr_array_new();

  self->flush_lines = 0;
  self->flush_timeout = 10000;  /* 10 seconds */
  self->mark_freq = 1200;	/* 20 minutes */
  self->stats_freq = 600;
  self->chain_hostnames = 1;
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
  self->recv_zone_offset = -1;
  self->send_zone_offset = -1;
  self->keep_timestamp = TRUE;

  configuration = self;

  if ((cfg = fopen(fname, "r")) != NULL)
    {
      lex_init(cfg);
      res = yyparse();
      fclose(cfg);
      if (!res)
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
cfg_remove_filter(gpointer key, gpointer value, gpointer user_data)
{
  LogFilterRule *s = (LogFilterRule *) value;
  
  log_filter_unref(s);
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
  
  g_free(self->file_template_name);
  g_free(self->proto_template_name);  
  log_template_unref(self->file_template);
  log_template_unref(self->proto_template);
  
  log_center_unref(self->center);
  
  g_hash_table_foreach_remove(self->sources, cfg_remove_pipe, NULL);
  g_hash_table_foreach_remove(self->destinations, cfg_remove_pipe, NULL);
  g_hash_table_foreach_remove(self->filters, cfg_remove_filter, NULL);
  g_hash_table_foreach_remove(self->templates, cfg_remove_template, NULL);
  
  for (i = 0; i < self->connections->len; i++)
    {
      log_connection_free(g_ptr_array_index(self->connections, i));
      g_ptr_array_remove_fast(self->connections, 0);
    }
    
  g_hash_table_destroy(self->sources);
  g_hash_table_destroy(self->destinations);
  g_hash_table_destroy(self->filters);
  g_hash_table_destroy(self->templates);
  g_ptr_array_free(self->connections, TRUE);
  if (self->bad_hostname_compiled)
    regfree(&self->bad_hostname);
  g_free(self->bad_hostname_re);
  g_free(self->dns_cache_hosts);
  g_free(self);
}

GlobalConfig *
cfg_reload_config(gchar *fname, GlobalConfig *cfg)
{
  PersistentConfig *persist;
  GlobalConfig *new_cfg;
  

  new_cfg = cfg_new(fname);
  if (!new_cfg)
    {
      msg_error("Error parsing configuration",
                evt_tag_str(EVT_TAG_FILENAME, fname),
                NULL);
      return cfg;
    }

  persist = persist_config_new();
  cfg_deinit(cfg, persist);

  if (cfg_init(new_cfg, persist))
    {
      msg_verbose("New configuration initialized", NULL);
      cfg_free(cfg);
      persist_config_free(persist);
      return new_cfg;
    }
  else
    {
      msg_error("Error initializing new configuration, reverting to old config", NULL);
      cfg_init(cfg, persist);
      persist_config_free(persist);
      return cfg;
    }
}

/* PersistentConfig */

struct _PersistentConfig
{
  GHashTable *keys;
};

typedef struct _PersistentConfigEntry
{
  gpointer value;
  GDestroyNotify destroy;
  /* this value is a string and should be saved */
  gboolean survive_across_restarts:1;
} PersistentConfigEntry;

static void
persist_config_entry_free(PersistentConfigEntry *self)
{
  if (self->destroy)
    self->destroy(self->value);
  g_free(self);
}

static PersistentConfigEntry *
persist_config_add_entry(PersistentConfig *self, gchar *name, gpointer value, GDestroyNotify destroy)
{
  PersistentConfigEntry *p;
  
  if (self && value)
    {
      if (g_hash_table_lookup(self->keys, name))
        {
          msg_error("Internal error, duplicate configuration elements refer to the same persistent config", 
                    evt_tag_str("name", name),
                    NULL);
          destroy(value);
          return NULL;
        }
  
      p = g_new0(PersistentConfigEntry, 1);
  
      p->value = value;
      p->destroy = destroy;
  
      g_hash_table_insert(self->keys, g_strdup(name), p);
      return p;
    }
  return NULL;
}

void
persist_config_add(PersistentConfig *self, gchar *name, gpointer value, GDestroyNotify destroy)
{
  persist_config_add_entry(self, name, value, destroy);
}

/**
 * persist_config_add_survivor:
 *
 * Add a "surviving" value, a value that is remembered accross restarts.
 **/
void
persist_config_add_survivor(PersistentConfig *self, gchar *name, gchar *value)
{
  PersistentConfigEntry *p;
  
  p = persist_config_add_entry(self, name, g_strdup(value), g_free);
  if (p)
    {
      p->survive_across_restarts = TRUE;
    }
}

gpointer
persist_config_fetch(PersistentConfig *self, gchar *name)
{
  gpointer res = NULL;
  gchar *orig_key;
  PersistentConfigEntry *p;
  gpointer tmp1, tmp2;
  
  if (self && g_hash_table_lookup_extended(self->keys, name, &tmp1, &tmp2))
    {
      orig_key = (gchar *) tmp1;
      p = (PersistentConfigEntry *) tmp2;
      res = p->value;
      g_hash_table_steal(self->keys, name);
      g_free(orig_key);
      g_free(p);
    }
  return res;
}

static gboolean
persist_config_write_string(FILE *persist_file, gchar *str)
{
  guint32 length;
  
  length = htonl(strlen(str));
  if (fwrite(&length, 1, sizeof(length), persist_file) == sizeof(length) &&
      fwrite(str, 1, ntohl(length), persist_file) == ntohl(length))
    {
      return TRUE;
    }
  return FALSE;
}

static gboolean
persist_config_read_string(FILE *persist_file, gchar **str)
{
  guint32 length;
  
  if (fread(&length, 1, sizeof(length), persist_file) != sizeof(length))
    return FALSE;
  length = ntohl(length);
  if (length > 4096)
    return FALSE;
  *str = g_malloc(length + 1);
  (*str)[length] = 0;
  if (fread(*str, 1, length, persist_file) != length)
    {
      g_free(*str);
      return FALSE;
    }
  return TRUE;
}

static void
persist_config_save_value(gchar *key, PersistentConfigEntry *entry, FILE *persist_file)
{
  if (entry->survive_across_restarts)
    {
      /* NOTE: we ignore errors here, as we cannot bail out from the
       * g_hash_table_foreach() loop anyway. */
      
      persist_config_write_string(persist_file, key);
      persist_config_write_string(persist_file, (gchar *) entry->value);
    }
}

void
persist_config_save(PersistentConfig *self)
{
  FILE *persist_file;
  
  persist_file = fopen(PATH_PERSIST_CONFIG, "w");
  if (persist_file)
    {
      if (fwrite("SLP1", 1, 4, persist_file) < 0)
        {
          fclose(persist_file);
          goto error;
        }
      g_hash_table_foreach(self->keys, (GHFunc) persist_config_save_value, persist_file);
      fclose(persist_file);
      return;
    }
 error:
  msg_error("Error saving persistent configuration file",
            evt_tag_str("name", PATH_PERSIST_CONFIG),
            NULL);
  
}

void
persist_config_load(PersistentConfig *self)
{
  FILE *persist_file;
  
  persist_file = fopen(PATH_PERSIST_CONFIG, "r");
  if (persist_file)
    {
      gchar magic[4];
      gchar *key, *value;
      
      if (fread(magic, 1, sizeof(magic), persist_file) != 4)
        {
          msg_error("Error loading persistent configuration file",
                    evt_tag_str("name", PATH_PERSIST_CONFIG),
                    NULL);
          goto close_and_exit;
        }
      if (memcmp(magic, "SLP1", 4) != 0)
        {
          msg_error("Persistent configuration file is in invalid format", NULL);
          goto close_and_exit;
        }
      while (persist_config_read_string(persist_file, &key))
        {
          if (persist_config_read_string(persist_file, &value))
            {
              /* add a non-surviving entry, thus each value is only
               * written/read once unless the code readds it, this is needed
               * to limit the size of the persistent configuration file */
              
              persist_config_add(self, key, value, g_free);
            }
          else
            {
              g_free(key);
              break;
            }
          g_free(key);
        }
 close_and_exit:
      fclose(persist_file);
    }
}

PersistentConfig *
persist_config_new(void)
{
  PersistentConfig *self = g_new0(PersistentConfig, 1);
  
  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) persist_config_entry_free);
  return self;
}

void 
persist_config_free(PersistentConfig *self)
{
  g_hash_table_destroy(self->keys);
  g_free(self);
}

