/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

GlobalConfig *configuration;

gboolean
cfg_timezone_value(gchar *tz, glong *timezone)
{
  if ((*tz == '+' || *tz == '-') && strlen(tz) == 6 && 
      isdigit((int) *(tz+1)) && isdigit((int) *(tz+2)) && (*(tz+3) == ':') && isdigit((int) *(tz+4)) && isdigit((int) *(tz+5)))
    {
      /* timezone offset */
      gint sign = *tz == '-' ? 1 : -1;
      gint hours, mins;
      tz++;
      
      hours = (*tz - '0') * 10 + *(tz+1) - '0';
      mins = (*(tz+3) - '0') * 10 + *(tz+4) - '0';
      if (hours <= 12 && mins <= 60)
        {
          *timezone = sign * (hours * 3600 + mins * 60);
          return TRUE;
        }
    }
  msg_error("Bogus timezone spec, must be in the format [+-]HHMM",
            evt_tag_str("value", tz),
            NULL);
  return FALSE;
}

gint
cfg_tz_convert_value(gchar *convert)
{
  if (strcmp(convert, "gmt") == 0)
    return TZ_CNV_GMT;
  else if (strcmp(convert, "local") == 0)
    return TZ_CNV_LOCAL;
  else if (strcmp(convert, "orig") == 0 || strcmp(convert, "original") == 0 || strcmp(convert, "keep"))
    return TZ_CNV_ORIG;
  else
    {
      msg_error("Invalid tz_convert() value",
                evt_tag_str("value", convert),
                NULL);
      return TZ_CNV_ORIG;
    }
}

gint
cfg_ts_format_value(gchar *format)
{
  if (strcmp(format, "rfc3164") == 0 || strcmp(format, "bsd") == 0)
    return TS_FMT_BSD;
  else if (strcmp(format, "rfc3339") == 0 || strcmp(format, "iso") == 0)
    return TS_FMT_ISO;
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
    msg_notice("Error resolving user",
               evt_tag_str("user", owner),
               NULL);
}

void
cfg_file_group_set(GlobalConfig *self, gchar *group)
{
  if (!resolve_group(group, &self->file_gid))
    msg_notice("Error resolving group",
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
    msg_notice("Error resolving user",
               evt_tag_str("user", owner),
               NULL);
}

void
cfg_dir_group_set(GlobalConfig *self, gchar *group)
{
  if (!resolve_group(group, &self->dir_gid))
    msg_notice("Error resolving group",
               evt_tag_str("group", group),
               NULL);
}

void
cfg_dir_perm_set(GlobalConfig *self, gint perm)
{
  self->dir_perm = perm;
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
  cfg->file_template = cfg_lookup_template(cfg, cfg->file_template_name);
  cfg->proto_template = cfg_lookup_template(cfg, cfg->proto_template_name);
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

  self->sync_freq = 0;
  self->mark_freq = 1200;	/* 20 minutes */
  self->stats_freq = 600;
  self->chain_hostnames = 1;
  self->use_fqdn = 0;
  self->use_dns = 1;
  self->time_reopen = 60;
  self->time_reap = 60;

  self->log_fifo_size = 100;
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
  self->tz_convert = TZ_CNV_LOCAL;
  self->keep_timestamp = FALSE;

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
} PersistentConfigEntry;

static void
persist_config_entry_free(PersistentConfigEntry *self)
{
  if (self->destroy)
    self->destroy(self->value);
  g_free(self);
}

void 
persist_config_add(PersistentConfig *self, gchar *name, gpointer value, GDestroyNotify destroy)
{
  PersistentConfigEntry *p;
  
  if (self)
    {
      g_assert(!g_hash_table_lookup(self->keys, name));
  
      p = g_new0(PersistentConfigEntry, 1);
  
      p->value = value;
      p->destroy = destroy;
  
      g_hash_table_insert(self->keys, g_strdup(name), p);
    }
}

gpointer
persist_config_fetch(PersistentConfig *self, gchar *name)
{
  gpointer res = NULL;
  gchar *orig_key;
  PersistentConfigEntry *p;
  
  if (self && g_hash_table_lookup_extended(self->keys, name, (gpointer *) &orig_key, (gpointer *) &p))
    {
      res = p->value;
      g_hash_table_steal(self->keys, name);
      g_free(orig_key);
      g_free(p);
    }
  return res;
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

