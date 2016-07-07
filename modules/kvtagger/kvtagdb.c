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

#include "kvtagdb.h"
#include "atomic.h"
#include <string.h>

struct _KVTagDB
{
  GAtomicCounter ref_cnt;
  GArray *data;
  GHashTable *index;
  gboolean is_data_indexed;
};

typedef struct _element_range
{
  gsize offset;
  gsize length;
} element_range;

static gint
_tagrecord_cmp(gconstpointer k1, gconstpointer k2)
{
  TagRecord *r1 = (TagRecord *)k1;
  TagRecord *r2 = (TagRecord *)k2;

  return strcmp(r1->selector, r2->selector);
}

void
kvtagdb_index(KVTagDB *self)
{
  if (self->data->len > 0)
    {
      g_array_sort(self->data, _tagrecord_cmp);
      gsize range_start = 0;
      TagRecord range_start_tag = g_array_index(self->data, TagRecord, 0);

      for (gsize i = 1; i < self->data->len; ++i)
        {
          TagRecord current_record = g_array_index(self->data, TagRecord, i);

          if (_tagrecord_cmp(&range_start_tag, &current_record))
            {
              element_range *current_range = g_new(element_range, 1);
              current_range->offset = range_start;
              current_range->length = i - range_start;

              g_hash_table_insert(self->index, range_start_tag.selector, current_range);

              range_start_tag = current_record;
              range_start = i;
            }
        }

      {
        element_range *last_range = g_new(element_range, 1);
        last_range->offset = range_start;
        last_range->length = self->data->len - range_start;
        g_hash_table_insert(self->index, range_start_tag.selector, last_range);
      }
      self->is_data_indexed = TRUE;
    }
}

static void
_ensure_indexed_db(KVTagDB *self)
{
  if (!self->is_data_indexed)
    kvtagdb_index(self);  
}

static void
_tag_record_free(gpointer p)
{
  TagRecord *rec = (TagRecord *)p;
  g_free(rec->selector);
  g_free(rec->name);
  g_free(rec->value);
}

static void
_new(KVTagDB *self)
{
  self->data = g_array_new(FALSE, FALSE, sizeof(TagRecord));
  g_array_set_clear_func(self->data, _tag_record_free);
  self->index = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
  self->is_data_indexed = FALSE;
  g_atomic_counter_set(&self->ref_cnt, 1);
}

static void
_free(KVTagDB *self)
{
  if (self->index)
    g_hash_table_unref(self->index);
  if (self->data)
    g_array_unref(self->data);
}

KVTagDB*
kvtagdb_new()
{
  KVTagDB *self = g_new0(KVTagDB, 1);
    
  _new(self);  

  return self;
}

KVTagDB*
kvtagdb_ref(KVTagDB *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);
  g_atomic_counter_inc(&self->ref_cnt);

  return self;
}

void
kvtagdb_unref(KVTagDB *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));
  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    {
      kvtagdb_free(self);
    }
}

static element_range*
_get_range_of_tags(KVTagDB *const self, const gchar *const selector)
{
  _ensure_indexed_db(self);
  return (element_range *)g_hash_table_lookup(self->index, selector);
}

void
kvtagdb_purge(KVTagDB *self)
{
  g_hash_table_remove_all(self->index);
  if (self->data->len > 0)
    self->data = g_array_remove_range(self->data, 0, self->data->len);
}

void
kvtagdb_free(KVTagDB *self)
{
  if (self)
    {
      _free(self);
      g_free(self);
    }
}

void
kvtagdb_insert(KVTagDB *self, const TagRecord *record)
{
  g_array_append_val(self->data, *record);
  self->is_data_indexed = FALSE;
}

gboolean
kvtagdb_contains(KVTagDB *self, const gchar *selector)
{
  _ensure_indexed_db(self);
  return (_get_range_of_tags(self, selector) != NULL);
}

gsize
kvtagdb_number_of_tags(KVTagDB *self, const gchar *selector)
{
  _ensure_indexed_db(self);

  gsize n = 0;
  element_range *range = _get_range_of_tags(self, selector);

  if (range)
    n = range->length;

  return n;
}

void
kvtagdb_foreach_tag(KVTagDB *self, const gchar *selector, KVTAGDB_TAG_CB callback, gpointer arg)
{
  _ensure_indexed_db(self);

  element_range *tag_range = _get_range_of_tags(self, selector);

  if (!tag_range)
    return;

  for (gsize i = tag_range->offset; i < tag_range->offset + tag_range->length; ++i)
    {
      TagRecord record = g_array_index(self->data, TagRecord, i);
      callback(arg, &record);
    }
}

gboolean
kvtagdb_is_indexed(KVTagDB *self)
{
  return self->is_data_indexed;
}

gboolean
kvtagdb_is_loaded(KVTagDB *self)
{
  return (self->data != NULL && self->data->len > 0);
}

GList *
kvtagdb_get_selectors(KVTagDB *self)
{
  _ensure_indexed_db(self);
  return g_hash_table_get_keys(self->index);
}

gboolean
kvtagdb_import(KVTagDB *self, FILE *fp, TagRecordScanner *scanner)
{
  gchar line[3072];
  const TagRecord *next_record;
  while(fgets(line, 3072, fp))
    {
      size_t line_length = strlen(line);
      if (line[line_length-1] == '\n')
        --line_length;
      line[line_length] = '\0';
      next_record = scanner->get_next(scanner, line);
      if (!next_record)
        {
          kvtagdb_purge(self);
          return FALSE;
        }
      kvtagdb_insert(self, next_record);
    }
  kvtagdb_index(self);
  return TRUE;
}
