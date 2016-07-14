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
  gchar *prefix;
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
kvtagger_set_prefix(LogParser *p, const gchar *prefix)
{
  KVTagger *self = (KVTagger *)p;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
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
  const gchar *selector = NULL;

  log_template_format(self->selector_template, msg, NULL, LTZ_LOCAL, 0, NULL, selector_str);

  if (kvtagdb_contains(self->tagdb, selector_str->str))
    selector = selector_str->str;
  else if (_is_default_selector_set(self))
    selector = self->default_selector;

  if (selector)
    kvtagdb_foreach_tag(self->tagdb, selector, _tag_message, (gpointer)msg);

  g_string_free(selector_str, TRUE);

  return TRUE;
}

static void
_replace_template(LogTemplate **old_template, LogTemplate *new_template)
{
  log_template_unref(*old_template);
  *old_template = log_template_ref(new_template);
}

static void
_replace_tagdb(KVTagDB **old_db, KVTagDB *new_db)
{
  kvtagdb_unref(*old_db);
  *old_db = kvtagdb_ref(new_db);
}

static LogPipe *
kvtagger_parser_clone(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;
  KVTagger *cloned = (KVTagger *)kvtagger_parser_new(s->cfg);

  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));
  _replace_tagdb(&cloned->tagdb, self->tagdb);
  _replace_template(&cloned->selector_template, self->selector_template);
  kvtagger_set_prefix(&cloned->super, self->prefix);
  kvtagger_set_filename(&cloned->super, self->filename);
  kvtagger_set_database_default_selector(&cloned->super, self->default_selector);

  return &cloned->super.super;
}

static void
kvtagger_parser_free(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;

  kvtagdb_unref(self->tagdb);
  g_free(self->filename);
  g_free(self->prefix);
  log_template_unref(self->selector_template);
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

const gchar*
_get_filename_extension(const gchar *filename)
{
   gsize filename_len = strlen(filename);

   if (filename_len < 3)
     return NULL;

   gsize diff = filename_len - 3;
   const gchar *extension = filename + diff;   
   return extension;
}

static TagRecordScanner*
_get_scanner(KVTagger *self)
{
  const gchar *type = _get_filename_extension(self->filename);
  TagRecordScanner *scanner = create_tag_record_scanner_by_type(type);

  if (!scanner)
    {
      msg_error("Unknown file extension", evt_tag_str("filename", self->filename));
      return NULL;
    }

  tag_record_scanner_set_name_prefix(scanner, self->prefix);

  return scanner;
}

static gboolean
_load_tagdb(KVTagger *self)
{
  TagRecordScanner *scanner = _get_scanner(self);

  if (!scanner)
    return FALSE;

  FILE *f = _open_data_file(self->filename);
  if (!f)
    {
      msg_error("Error loading kvtagger database", evt_tag_str("filename", self->filename));
      tag_record_scanner_free(scanner);
      return FALSE;
    }

  gboolean tag_db_loaded = kvtagdb_import(self->tagdb, f, scanner);
  tag_record_scanner_free(scanner);

  fclose(f);
  if (!tag_db_loaded)
    {
      msg_error("Error while parsing kvtagger database");
      return FALSE;
    }

  return TRUE;
}

static gboolean
_is_initialized(KVTagger *self)
{
  return kvtagdb_is_loaded(self->tagdb);
}

static gboolean
_first_init(KVTagger *self)
{
  if (self->filename == NULL)
    {
      msg_error("No database file set.");
      return FALSE;
    }

  if (!_load_tagdb(self))
    {
      msg_error("Failed to load the database file.");
      return FALSE;
    }

  return TRUE;
}

static gboolean
kvtagger_parser_init(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;

  if (_is_initialized(self) || _first_init(self))
    return log_parser_init_method(s);

  return FALSE;
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
  self->super.super.free_fn = kvtagger_parser_free;
  self->super.super.init = kvtagger_parser_init;
  self->default_selector = NULL;
  self->prefix = NULL;

  return &self->super;
}
