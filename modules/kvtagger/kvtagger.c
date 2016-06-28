/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2015 Balázs Scheidler
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

#include <stdio.h>
#include <string.h>

typedef struct element_range
{
  gsize offset;
  gsize length;
} element_range;

typedef struct KVTagDB
{
  GArray *data;
  GHashTable *index;
} KVTagDB;

typedef struct KVTagger
{
  LogParser super;
  KVTagDB tagdb;
  LogTemplate *selector_template;
  gchar *default_selector;
  gchar *filename;
  TagRecordScanner *scanner;
} KVTagger;

static KVTagDB
kvtagdb_ref(KVTagDB *s)
{
  return (KVTagDB){.data = g_array_ref(s->data), .index = g_hash_table_ref(s->index)};
}

static void
kvtagdb_unref(KVTagDB *s)
{
  g_array_unref(s->data);
  g_hash_table_unref(s->index);
}

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

static inline element_range *
kvtagger_lookup_tag(KVTagger *const self, const gchar *const selector)
{
  return g_hash_table_lookup(self->tagdb.index, selector);
}

gboolean
_is_default_selector_set(KVTagger *self)
{
  return (self->default_selector != NULL);
}

element_range *
_get_range_of_tags(KVTagger *self, GString *selector)
{
  element_range *position_of_tags_to_be_inserted = kvtagger_lookup_tag(self, selector->str);
  if (position_of_tags_to_be_inserted == NULL && _is_default_selector_set(self))
    {
      position_of_tags_to_be_inserted = kvtagger_lookup_tag(self, self->default_selector);
    }
  return position_of_tags_to_be_inserted;
}

void
_tag_message(KVTagger *self, LogMessage *msg, element_range *position_of_tags_to_be_inserted)
{
  for (gsize i = position_of_tags_to_be_inserted->offset;
       i < position_of_tags_to_be_inserted->offset + position_of_tags_to_be_inserted->length;
       ++i)
    {
      tag_record matching_tag = g_array_index(self->tagdb.data, tag_record, i);
      log_msg_set_value_by_name(msg, matching_tag.name, matching_tag.value, -1);
    }
}

static gboolean
kvtagger_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options,
                        const gchar *input, gsize input_len)
{
  KVTagger *self = (KVTagger *)s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  GString *selector = g_string_new(NULL);

  log_template_format(self->selector_template, msg, NULL, LTZ_LOCAL, 0, NULL, selector);
  element_range* position_of_tags_to_be_inserted = _get_range_of_tags(self, selector);
  if (position_of_tags_to_be_inserted != NULL)
    {
      _tag_message(self, msg, position_of_tags_to_be_inserted);
    }

  return TRUE;
}

static LogPipe *
kvtagger_parser_clone(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;
  KVTagger *cloned = (KVTagger *)kvtagger_parser_new(s->cfg);

  cloned->super.template = log_template_ref(self->super.template);
  cloned->tagdb = kvtagdb_ref(&self->tagdb);

  return &cloned->super.super;
}

static void
kvtagger_parser_free(LogPipe *s)
{
  KVTagger *self = (KVTagger *)s;

  g_free(self->filename);
  g_free(self->scanner);
  log_parser_free_method(s);
}

static gint
_kv_comparer(gconstpointer k1, gconstpointer k2)
{
  tag_record *r1 = (tag_record *)k1;
  tag_record *r2 = (tag_record *)k2;
  return strcmp(r1->selector, r2->selector);
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

static GArray *
_parse_input_file(KVTagger *self, FILE *file)
{
  GArray *data = self->scanner->get_parsed_records(self->scanner, file);

  return data;
}

static void
kvtagger_sort_array_by_key(GArray *array)
{
  g_array_sort(array, _kv_comparer);
}

static GHashTable *
kvtagger_classify_array(const GArray *const record_array)
{
  GHashTable *index = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

  if (record_array->len > 0)
    {
      gsize range_start = 0;
      tag_record range_start_tag = g_array_index(record_array, tag_record, 0);

      for (gsize i = 1; i < record_array->len; ++i)
        {
          tag_record current_record = g_array_index(record_array, tag_record, i);

          if (_kv_comparer(&range_start_tag, &current_record))
            {
              element_range *current_range = g_new(element_range, 1);
              current_range->offset = range_start;
              current_range->length = i - range_start;

              g_hash_table_insert(index, range_start_tag.selector, current_range);

              range_start_tag = current_record;
              range_start = i;
            }
        }

      {
        element_range *last_range = g_new(element_range, 1);
        last_range->offset = range_start;
        last_range->length = record_array->len - range_start;
        g_hash_table_insert(index, range_start_tag.selector, last_range);
      }
    }

  return index;
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

  self->tagdb.data = _parse_input_file(self, f);

  fclose(f);
  if (!self->tagdb.data)
    {
      msg_error("Error while parsing kvtagger database");
      return FALSE;
    }
  kvtagger_sort_array_by_key(self->tagdb.data);
  self->tagdb.index = kvtagger_classify_array(self->tagdb.data);

  return TRUE;
}

static const gchar *
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
      self->tagdb.data = restored_kvtagdb->data;
      self->tagdb.index = restored_kvtagdb->index;
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
_save_kvtagdb_to_globalconfig(KVTagger *self, GlobalConfig *cfg)
{
  KVTagDB *tagdb_shallow_copy = g_new0(KVTagDB, 1);
  *tagdb_shallow_copy = kvtagdb_ref(&self->tagdb);

  cfg_persist_config_add(cfg, _format_persist_name(self), tagdb_shallow_copy, kvtagdb_unref, FALSE);
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

  self->super.super.clone = kvtagger_parser_clone;
  self->super.super.deinit = kvtagger_parser_deinit;
  self->super.super.free_fn = kvtagger_parser_free;
  self->super.super.init = kvtagger_parser_init;
  self->scanner = NULL;
  self->default_selector = NULL;

  return &self->super;
}
