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

#include "context-info-db.h"
#include "atomic.h"
#include "messages.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

struct _ContextInfoDB
{
  GAtomicCounter ref_cnt;
  GArray *data;
  GHashTable *index;
  gboolean is_data_indexed;
  gboolean is_ordering_enabled;
  GList *ordered_selectors;
  gboolean ignore_case;
};

typedef struct _element_range
{
  gsize offset;
  gsize length;
} element_range;

static gint
_contextual_data_record_cmp(gconstpointer k1, gconstpointer k2)
{
  ContextualDataRecord *r1 = (ContextualDataRecord *) k1;
  ContextualDataRecord *r2 = (ContextualDataRecord *) k2;

  return strcmp(r1->selector->str, r2->selector->str);
}

static gint
_g_strcmp(gconstpointer a, gconstpointer b)
{
  return g_strcmp0((const gchar *) a, (const gchar *) b);
}

static gint
_g_strcasecmp(gconstpointer a, gconstpointer b)
{
  if (!a || !b)
    return 1;
  return g_ascii_strcasecmp((const gchar *)a, (const gchar *)b);
}

static gint
_contextual_data_record_case_cmp(gconstpointer k1, gconstpointer k2)
{
  ContextualDataRecord *r1 = (ContextualDataRecord *) k1;
  ContextualDataRecord *r2 = (ContextualDataRecord *) k2;

  return _g_strcasecmp(r1->selector->str, r2->selector->str);
}

void
context_info_db_enable_ordering(ContextInfoDB *self)
{
  self->is_ordering_enabled = TRUE;
}

GList *
context_info_db_ordered_selectors(ContextInfoDB *self)
{
  return self->ordered_selectors;
}

void
context_info_db_index(ContextInfoDB *self)
{
  GCompareFunc record_cmp = self->ignore_case ? _contextual_data_record_case_cmp : _contextual_data_record_cmp;

  if (self->data->len > 0)
    {
      g_array_sort(self->data, record_cmp);
      gsize range_start = 0;
      ContextualDataRecord *range_start_record =
        &g_array_index(self->data, ContextualDataRecord, 0);

      for (gsize i = 1; i < self->data->len; ++i)
        {
          ContextualDataRecord *current_record =
            &g_array_index(self->data, ContextualDataRecord, i);

          if (record_cmp(range_start_record, current_record))
            {
              element_range *current_range = g_new(element_range, 1);
              current_range->offset = range_start;
              current_range->length = i - range_start;

              g_hash_table_insert(self->index, range_start_record->selector->str,
                                  current_range);

              range_start_record = current_record;
              range_start = i;
            }
        }

      {
        element_range *last_range = g_new(element_range, 1);
        last_range->offset = range_start;
        last_range->length = self->data->len - range_start;
        g_hash_table_insert(self->index, range_start_record->selector->str,
                            last_range);
      }
      self->is_data_indexed = TRUE;
    }
}

static void
_ensure_indexed_db(ContextInfoDB *self)
{
  if (!self->is_data_indexed)
    context_info_db_index(self);
}

static void
_record_free(gpointer p)
{
  ContextualDataRecord *rec = (ContextualDataRecord *) p;
  contextual_data_record_clean(rec);
}

static gboolean
_strcase_eq(gconstpointer a, gconstpointer b)
{
  return _g_strcasecmp(a,b) == 0;
}

static guint
_str_case_insensitive_djb2_hash(const gchar *str)
{
  guint hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + g_ascii_toupper(c);

  return hash;
}

static guint
_strcase_hash(gconstpointer value)
{
  return _str_case_insensitive_djb2_hash((const gchar *)value);
}

static void
_free_array(GArray *array)
{
  for (gsize i = 0; i < array->len; ++i)
    {
      ContextualDataRecord current_record =
        g_array_index(array, ContextualDataRecord, i);
      _record_free(&current_record);
    }
  g_array_free(array, TRUE);
}

static void
_free(ContextInfoDB *self)
{
  if (self->index)
    {
      g_hash_table_unref(self->index);
    }
  if (self->data)
    {
      _free_array(self->data);
    }
  if (self->ordered_selectors)
    {
      g_list_free(self->ordered_selectors);
    }
}


static element_range *
_get_range_of_records(ContextInfoDB *self, const gchar *selector)
{
  _ensure_indexed_db(self);
  return (element_range *) g_hash_table_lookup(self->index, selector);
}

void
context_info_db_purge(ContextInfoDB *self)
{
  g_hash_table_remove_all(self->index);
  if (self->data->len > 0)
    self->data = g_array_remove_range(self->data, 0, self->data->len);
}


void
context_info_db_insert(ContextInfoDB *self,
                       const ContextualDataRecord *record)
{
  g_array_append_val(self->data, *record);
  self->is_data_indexed = FALSE;
  if (self->is_ordering_enabled && !g_list_find_custom(self->ordered_selectors, record->selector->str, _g_strcmp))
    self->ordered_selectors = g_list_append(self->ordered_selectors, record->selector->str);
}

gboolean
context_info_db_contains(ContextInfoDB *self, const gchar *selector)
{
  if (!selector)
    return FALSE;

  _ensure_indexed_db(self);
  return (_get_range_of_records(self, selector) != NULL);
}

gsize
context_info_db_number_of_records(ContextInfoDB *self,
                                  const gchar *selector)
{
  _ensure_indexed_db(self);

  gsize n = 0;
  element_range *range = _get_range_of_records(self, selector);

  if (range)
    n = range->length;

  return n;
}

void
context_info_db_foreach_record(ContextInfoDB *self, const gchar *selector,
                               ADD_CONTEXT_INFO_CB callback, gpointer arg)
{
  _ensure_indexed_db(self);

  element_range *record_range = _get_range_of_records(self, selector);

  if (!record_range)
    return;

  for (gsize i = record_range->offset;
       i < record_range->offset + record_range->length; ++i)
    {
      ContextualDataRecord *record =
        &g_array_index(self->data, ContextualDataRecord, i);
      callback(arg, record);
    }
}

gboolean
context_info_db_is_indexed(const ContextInfoDB *self)
{
  return self->is_data_indexed;
}

gboolean
context_info_db_is_loaded(const ContextInfoDB *self)
{
  return (self->data != NULL && self->data->len > 0);
}

GList *
context_info_db_get_selectors(ContextInfoDB *self)
{
  _ensure_indexed_db(self);
  return g_hash_table_get_keys(self->index);
}

static void
_truncate_eol(gchar *line, gsize line_len)
{
  if (line_len >= 2 && line[line_len - 2] == '\r' && line[line_len - 1] == '\n')
    line[line_len - 2] = '\0';
  else if (line_len >= 1 && line[line_len - 1] == '\n')
    line[line_len - 1] = '\0';
}

static gboolean
_get_line_without_eol(gchar **line_buf, gsize *line_buf_len, FILE *fp)
{
  gssize n;
  if ((n = getline(line_buf, line_buf_len, fp)) == -1)
    return FALSE;

  _truncate_eol(*line_buf, n);
  *line_buf_len = strlen(*line_buf);
  return TRUE;
}

gboolean
context_info_db_import(ContextInfoDB *self, FILE *fp,
                       ContextualDataRecordScanner *scanner)
{
  size_t line_buf_len;
  gchar *line_buf = NULL;
  const ContextualDataRecord *next_record;

  while (_get_line_without_eol(&line_buf, &line_buf_len, fp))
    {
      if (line_buf_len == 0)
        continue;
      next_record = contextual_data_record_scanner_get_next(scanner, line_buf);
      if (!next_record)
        {
          context_info_db_purge(self);
          g_free(line_buf);
          return FALSE;
        }
      msg_trace("add-contextual-data(): adding database entry",
                evt_tag_str("selector", next_record->selector->str),
                evt_tag_str("name", next_record->name->str),
                evt_tag_str("value", next_record->value->str));
      context_info_db_insert(self, next_record);
    }

  g_free(line_buf);
  context_info_db_index(self);

  return TRUE;
}

ContextInfoDB *
context_info_db_new(gboolean ignore_case)
{
  ContextInfoDB *self = g_new0(ContextInfoDB, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);

  self->ignore_case = ignore_case;
  GEqualFunc str_eq = self->ignore_case ? _strcase_eq : g_str_equal;
  GHashFunc str_hash = self->ignore_case ? _strcase_hash : g_str_hash;
  self->data = g_array_new(FALSE, FALSE, sizeof(ContextualDataRecord));
  self->index = g_hash_table_new_full(str_hash, str_eq, NULL, g_free);
  return self;
}

ContextInfoDB *
context_info_db_ref(ContextInfoDB *self)
{
  if (self)
    {
      g_assert(g_atomic_counter_get(&self->ref_cnt) > 0);
      g_atomic_counter_inc(&self->ref_cnt);
    }

  return self;
}

void
context_info_db_unref(ContextInfoDB *self)
{
  if (self)
    {
      g_assert(g_atomic_counter_get(&self->ref_cnt));
      if (g_atomic_counter_dec_and_test(&self->ref_cnt))
        {
          _free(self);
          g_free(self);
        }
    }
}
