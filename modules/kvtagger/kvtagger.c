/*
 * Copyright (c) 2016 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "kvtagger.h"
#include "csv-tagrecord-scanner.h"
#include "logmsg/logmsg.h"
#include "logpipe.h"
#include "parser/parser-expr.h"
#include "reloc.h"
#include "tagrecord-scanner.h"
#include "template/templates.h"
#include "kvtagdb.h"

#include <stdio.h>
#include <string.h>


typedef struct KVTagger
{
  LogParser super;
  KVTagDB *tagdb;
  LogTemplate *selector_template;
  gchar *default_selector;
  gchar *filename;
  TagRecordScanner *scanner;
} KVTagger;

void
kvtagger_set_filename(LogParser *p, const gchar *filename)
{
  KVTagger *self = (KVTagger *)p;

  g_free(self->filename);
  self->filename = g_strdup(filename);
}

void
kvtagger_set_database_selector_template(LogParser *p, const gchar *selector)
{
  KVTagger *self = (KVTagger *)p;

  log_template_compile(self->selector_template, selector, NULL);
}

void
kvtagger_set_database_default_selector(LogParser *p, const gchar *default_selector)
{
  KVTagger *self = (KVTagger *)p;

  g_free(self->default_selector);
  self->default_selector = g_strdup(default_selector);
}

gboolean
_is_default_selector_set(KVTagger *self)
{
  return (self->default_selector != NULL);
}

void
_tag_message(gpointer pmsg, const TagRecord *tag)
{
  LogMessage *msg = (LogMessage *) pmsg;
  log_msg_set_value_by_name(msg, tag->name, tag->value, -1);
}

static gboolean
kvtagger_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options,
                        const gchar *input, gsize input_len)
{
  KVTagger *self = (KVTagger *)s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  GString *selector_str = g_string_new(NULL);
  const gchar *selector;

  log_template_format(self->selector_template, msg, NULL, LTZ_LOCAL, 0, NULL, selector_str);

  if (kvtagdb_contains(self->tagdb, selector_str->str))
    selector = selector_str->str;
  else if (_is_default_selector_set(self))
    selector = self->default_selector;

  kvtagdb_foreach_tag(self->tagdb, selector, _tag_message, (gpointer)msg);

  g_string_free(selector_str, TRUE);

  return TRUE;
}

static LogPipe *
kvtagger_parser_clone(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;
  KVTagger *cloned = (KVTagger *)kvtagger_parser_new(s->cfg);

  cloned->super.template = log_template_ref(self->super.template);
  cloned->tagdb = kvtagdb_ref(self->tagdb);

  return &cloned->super.super;
}

static void
kvtagger_parser_free(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;

  kvtagdb_unref(self->tagdb);
  g_free(self->filename);
  g_free(self->scanner);
  log_parser_free_method(s);
}

static gboolean
_is_relative_path(const gchar *filename)
{
  return (filename[0] != '/');
}

static gchar *
_complete_relative_path_with_config_path(const gchar *filename)
{
  return g_build_filename(get_installation_path_for(SYSLOG_NG_PATH_SYSCONFDIR), filename, NULL);
}

static FILE *
_open_data_file(const gchar *filename)
{
  FILE *f = NULL;

  if (_is_relative_path(filename))
    {
      gchar *absolute_path = _complete_relative_path_with_config_path(filename);
      f = fopen(absolute_path, "r");
      g_free(absolute_path);
    }
  else
    {
      f = fopen(filename, "r");
    }

  return f;
}

void
_parse_input_file(KVTagger *self, FILE *file)
{
  gchar line[3072];
  const TagRecord *next_record;
  while(fgets(line, 3072, file))
    {
      size_t line_length = strlen(line) - 1;
      line[line_length] = '\0';
      next_record = self->scanner->get_next(self->scanner, line);
      if (!next_record)
        {
          // Are we sure in this?
          kvtagdb_purge(self->tagdb);
          return;
        }
      kvtagdb_insert(self->tagdb, next_record);
    }
}

static gboolean
_prepare_scanner(KVTagger *self)
{
  gsize filename_len = strlen(self->filename);
  if (filename_len >= 3)
    {
      gsize diff = filename_len - 3;
      const gchar *extension = self->filename + diff;
      if (strcmp(extension, "csv") == 0)
        {

          GlobalConfig *cfg = log_pipe_get_config(&self->super.super);
          self->scanner = (TagRecordScanner *)csv_tagger_scanner_new(cfg);
        }
      else
        {
          return FALSE;
        }
    }
  else
    {
      return FALSE;
    }
  return TRUE;
}

static gboolean
kvtagger_create_lookup_table_from_file(KVTagger *self)
{
  if (!_prepare_scanner(self))
    {
      msg_error("Unknown file extension", evt_tag_str("filename", self->filename));
      return FALSE;
    }

  FILE *f = _open_data_file(self->filename);
  if (!f)
    {
      msg_error("Error loading kvtagger database", evt_tag_str("filename", self->filename));
      return FALSE;
    }

  _parse_input_file(self, f);
  if (kvtagdb_is_loaded(self->tagdb))
    kvtagdb_index(self->tagdb);

  fclose(f);
  if (!kvtagdb_is_indexed(self->tagdb))
    {
      msg_error("Error while parsing kvtagger database");
      return FALSE;
    }

  return TRUE;
}

static gchar *
_format_persist_name(const KVTagger *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name), "kvtagger(%s)", self->filename);
  return persist_name;
}

static gboolean
_restore_kvtagdb_from_globalconfig(KVTagger *self, GlobalConfig *cfg)
{
  KVTagDB *restored_kvtagdb = (KVTagDB *)cfg_persist_config_fetch(cfg, _format_persist_name(self));

  if (restored_kvtagdb)
    {
      if (self->tagdb != restored_kvtagdb)
        {
          kvtagdb_unref(self->tagdb);
          self->tagdb = restored_kvtagdb;
        }
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
kvtagger_parser_init(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->filename == NULL)
    {
      msg_error("No database file set.");
      return FALSE;
    }

  if (!kvtagger_create_lookup_table_from_file(self))
    {
      msg_error("Failed to load the database file, finding cached database...");

      if (_restore_kvtagdb_from_globalconfig(self, cfg))
        {
          msg_warning("Cached database found, falling back...");
        }
      else
        {
          msg_error("No cached database found, kvtagger initialization failed.");
          return FALSE;
        }
    }

  return log_parser_init_method(s);
}

static void
_destroy_notify_kvtagdb_unref(gpointer kvtagdb)
{
  kvtagdb_unref((KVTagDB *)kvtagdb);
}

static void
_save_kvtagdb_to_globalconfig(KVTagger *self, GlobalConfig *cfg)
{
  cfg_persist_config_add(cfg, _format_persist_name(self), kvtagdb_ref(self->tagdb), _destroy_notify_kvtagdb_unref, FALSE);
}

static gboolean
kvtagger_parser_deinit(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  _save_kvtagdb_to_globalconfig(self, cfg);

  return TRUE;
}

LogParser *
kvtagger_parser_new(GlobalConfig *cfg)
{
  KVTagger *self = g_new0(KVTagger, 1);

  log_parser_init_instance(&self->super, cfg);

  self->selector_template = log_template_new(cfg, NULL);

  self->super.process = kvtagger_parser_process;
  self->tagdb = kvtagdb_new();

  self->super.super.clone = kvtagger_parser_clone;
  self->super.super.deinit = kvtagger_parser_deinit;
  self->super.super.free_fn = kvtagger_parser_free;
  self->super.super.init = kvtagger_parser_init;
  self->scanner = NULL;
  self->default_selector = NULL;

  return &self->super;
}
