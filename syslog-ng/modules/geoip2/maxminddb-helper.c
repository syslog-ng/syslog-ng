/*
 * Copyright (c) 2017 Balabit
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

#include "maxminddb-helper.h"
#include "scratch-buffers.h"
#include <logmsg/logmsg.h>
#include <messages.h>

#define return_and_set_error_if(predicate, status)                     \
  if (predicate)                                                       \
    {                                                                  \
      *status = MMDB_INVALID_DATA_ERROR;                               \
      return NULL;                                                     \
    }

gchar *
mmdb_default_database(void)
{
  static const gchar *possible_paths[] =
  {
    "/usr/share/GeoIP/GeoLite2-City.mmdb", //centos, arch, fedora
    "/var/lib/GeoIP/GeoLite2-City.mmdb",   //ubuntu
  };

  for (gint i = 0; i <  G_N_ELEMENTS(possible_paths); ++i)
    {
      if (g_file_test(possible_paths[i], G_FILE_TEST_EXISTS))
        return g_strdup(possible_paths[i]);
    }

  return NULL;
}

gboolean
mmdb_open_database(const gchar *path, MMDB_s *database)
{
  int mmdb_status = MMDB_open(path, MMDB_MODE_MMAP, database);
  if (mmdb_status != MMDB_SUCCESS)
    {
      msg_error("MMDB_open",
                evt_tag_str("error", MMDB_strerror(mmdb_status)));
      return FALSE;
    }
  return TRUE;
}

void
append_mmdb_entry_data_to_gstring(GString *target, MMDB_entry_data_s *entry_data)
{

  switch (entry_data->type)
    {
    case MMDB_DATA_TYPE_UTF8_STRING:
      g_string_append_printf(target, "%.*s", entry_data->data_size, entry_data->utf8_string);
      break;
    case MMDB_DATA_TYPE_DOUBLE:
      g_string_append_printf(target, "%f", entry_data->double_value);
      break;
    case MMDB_DATA_TYPE_FLOAT:
      g_string_append_printf(target, "%f", (double)entry_data->float_value);
      break;
    case MMDB_DATA_TYPE_UINT16:
      g_string_append_printf(target, "%u", entry_data->uint16);
      break;
    case MMDB_DATA_TYPE_UINT32:
      g_string_append_printf(target, "%u", entry_data->uint32);
      break;
    case MMDB_DATA_TYPE_INT32:
      g_string_append_printf(target, "%d", entry_data->int32);
      break;
    case MMDB_DATA_TYPE_UINT64:
      g_string_append_printf(target, "%"G_GUINT64_FORMAT, entry_data->uint64);
      break;
    case MMDB_DATA_TYPE_BOOLEAN:
      g_string_append_printf(target, "%s", entry_data->boolean ? "true" : "false");
      break;
    case MMDB_DATA_TYPE_UINT128:
    case MMDB_DATA_TYPE_MAP:
    case MMDB_DATA_TYPE_BYTES:
    case MMDB_DATA_TYPE_ARRAY:
      g_assert_not_reached();
    default:
      g_assert_not_reached();
    }
}

static MMDB_entry_data_list_s *
mmdb_skip_tree(MMDB_entry_data_list_s *entry_data_list, gint *status);

static MMDB_entry_data_list_s *
mmdb_skip_map(MMDB_entry_data_list_s *entry_data_list, gint *status)
{
  guint32 size = entry_data_list->entry_data.data_size;

  for (entry_data_list = entry_data_list->next;
       size && entry_data_list; size--)
    {

      return_and_set_error_if(MMDB_DATA_TYPE_UTF8_STRING != entry_data_list->entry_data.type, status);

      entry_data_list = entry_data_list->next;
      entry_data_list = mmdb_skip_tree(entry_data_list, status);
      if (MMDB_SUCCESS != *status)
        {
          return NULL;
        }
    }

  return entry_data_list;
}

static MMDB_entry_data_list_s *
mmdb_skip_array(MMDB_entry_data_list_s *entry_data_list, gint *status)
{
  uint32_t size = entry_data_list->entry_data.data_size;

  for (entry_data_list = entry_data_list->next;
       size && entry_data_list; size--)
    {
      entry_data_list = mmdb_skip_tree(entry_data_list, status);

      if (MMDB_SUCCESS != *status)
        {
          return NULL;
        }
    }
  return entry_data_list;
}

static MMDB_entry_data_list_s *
mmdb_skip_tree(MMDB_entry_data_list_s *entry_data_list, gint *status)
{

  switch (entry_data_list->entry_data.type)
    {
    case MMDB_DATA_TYPE_MAP:
      entry_data_list = mmdb_skip_map(entry_data_list, status);
      break;
    case MMDB_DATA_TYPE_ARRAY:
      entry_data_list = mmdb_skip_array(entry_data_list, status);
      break;
    case MMDB_DATA_TYPE_BYTES:
    case MMDB_DATA_TYPE_BOOLEAN:
    case MMDB_DATA_TYPE_UINT128:
    case MMDB_DATA_TYPE_UTF8_STRING:
    case MMDB_DATA_TYPE_DOUBLE:
    case MMDB_DATA_TYPE_FLOAT:
    case MMDB_DATA_TYPE_UINT16:
    case MMDB_DATA_TYPE_UINT32:
    case MMDB_DATA_TYPE_UINT64:
    case MMDB_DATA_TYPE_INT32:
      entry_data_list = entry_data_list->next;
      break;

    default:
      *status = MMDB_INVALID_DATA_ERROR;
      return NULL;
    }

  *status = MMDB_SUCCESS;
  return entry_data_list;
}

static void
_geoip_log_msg_add_value(LogMessage *msg, GArray *path, GString *value)
{
  gchar *path_string = g_strjoinv(".", (gchar **)path->data);
  log_msg_set_value_by_name(msg, path_string, value->str, value->len);
  g_free(path_string);
}

static void
_print_preferred_string_for_lang(LogMessage *msg, MMDB_entry_data_s *entry_data, GArray *path,
                                 gchar *preferred_language)
{
  g_array_append_val(path, preferred_language);

  GString *value = scratch_buffers_alloc();
  g_string_printf(value, "%.*s",
                  entry_data->data_size,
                  entry_data->utf8_string);
  _geoip_log_msg_add_value(msg, path, value);
  g_array_remove_index(path, path->len-1);
}

static MMDB_entry_data_list_s *
check_language_and_maybe_insert(GString *key, gchar *preferred_language, LogMessage *msg,
                                MMDB_entry_data_list_s *entry_data_list, GArray *path, gint *status)
{
  if (!strcmp(key->str, preferred_language))
    {
      return_and_set_error_if(entry_data_list->entry_data.type != MMDB_DATA_TYPE_UTF8_STRING, status);

      _print_preferred_string_for_lang(msg, &entry_data_list->entry_data, path, preferred_language);
      entry_data_list = entry_data_list->next;
    }
  else
    {
      entry_data_list = mmdb_skip_tree(entry_data_list, status);
      if (MMDB_SUCCESS != *status)
        return NULL;
    }

  return entry_data_list;
}

static MMDB_entry_data_list_s *
select_language(gchar *preferred_language, LogMessage *msg,
                MMDB_entry_data_list_s *entry_data_list, GArray *path, gint *status)
{

  return_and_set_error_if(entry_data_list->entry_data.type != MMDB_DATA_TYPE_MAP, status)

  guint32 size = entry_data_list->entry_data.data_size;

  for (entry_data_list = entry_data_list->next;
       size && entry_data_list; size--)
    {
      return_and_set_error_if(entry_data_list->entry_data.type != MMDB_DATA_TYPE_UTF8_STRING, status);

      GString *key = scratch_buffers_alloc();
      g_string_printf(key, "%.*s",
                      entry_data_list->entry_data.data_size,
                      entry_data_list->entry_data.utf8_string);

      entry_data_list = entry_data_list->next;
      entry_data_list = check_language_and_maybe_insert(key, preferred_language, msg,
                                                        entry_data_list, path, status);
      if (MMDB_SUCCESS != *status)
        return NULL;

    }
  return entry_data_list;
}

static void
_index_array_in_path(GArray *path, guint32 _index, GString *indexer)
{
  g_string_printf(indexer, "%d", _index);
  ((gchar **)path->data)[path->len-1] = indexer->str;
}

MMDB_entry_data_list_s *
dump_geodata_into_msg_map(LogMessage *msg, MMDB_entry_data_list_s *entry_data_list, GArray *path, gint *status)
{
  guint32 size = entry_data_list->entry_data.data_size;

  for (entry_data_list = entry_data_list->next;
       size && entry_data_list; size--)
    {
      return_and_set_error_if(MMDB_DATA_TYPE_UTF8_STRING != entry_data_list->entry_data.type, status);

      GString *key = scratch_buffers_alloc();
      g_string_printf(key, "%.*s",
                      entry_data_list->entry_data.data_size,
                      entry_data_list->entry_data.utf8_string);

      g_array_append_val(path, key->str);
      entry_data_list = entry_data_list->next;

      if (!strcmp(key->str, "names"))
        entry_data_list = select_language("en", msg, entry_data_list, path, status);
      else
        entry_data_list = dump_geodata_into_msg(msg, entry_data_list, path, status);

      if (MMDB_SUCCESS != *status)
        return NULL;

      g_array_remove_index(path, path->len-1);
    }

  return entry_data_list;
}

MMDB_entry_data_list_s *
dump_geodata_into_msg_array(LogMessage *msg, MMDB_entry_data_list_s *entry_data_list, GArray *path, gint *status)
{
  guint32 size = entry_data_list->entry_data.data_size;
  guint32 _index = 0;
  GString *indexer = scratch_buffers_alloc();
  g_array_append_val(path, indexer->str);

  for (entry_data_list = entry_data_list->next;
       (_index < size) && entry_data_list;
       _index++)
    {
      _index_array_in_path(path, _index, indexer);
      entry_data_list = dump_geodata_into_msg(msg, entry_data_list, path, status);

      if (MMDB_SUCCESS != *status)
        return NULL;
    }
  g_array_remove_index(path, path->len-1);

  return entry_data_list;
}

static void G_GNUC_PRINTF(3, 4)
dump_geodata_into_msg_data(LogMessage *msg, GArray *path, gchar *fmt, ...)
{
  GString *value = scratch_buffers_alloc();
  va_list va;

  va_start(va, fmt);
  g_string_vprintf(value, fmt, va);
  va_end(va);

  _geoip_log_msg_add_value(msg, path, value);
}

MMDB_entry_data_list_s *
dump_geodata_into_msg(LogMessage *msg, MMDB_entry_data_list_s *entry_data_list, GArray *path, gint *status)
{
  switch (entry_data_list->entry_data.type)
    {
    case MMDB_DATA_TYPE_MAP:
      entry_data_list = dump_geodata_into_msg_map(msg, entry_data_list, path, status);
      if (MMDB_SUCCESS != *status)
        return NULL;
      break;

    case MMDB_DATA_TYPE_BYTES:
    case MMDB_DATA_TYPE_UINT128:
      g_assert_not_reached();

    case MMDB_DATA_TYPE_ARRAY:
      entry_data_list = dump_geodata_into_msg_array(msg, entry_data_list, path, status);
      if (MMDB_SUCCESS != *status)
        return NULL;
      break;
    case MMDB_DATA_TYPE_UTF8_STRING:
      dump_geodata_into_msg_data(msg, path, "%.*s", entry_data_list->entry_data.data_size,
                                 entry_data_list->entry_data.utf8_string);
      entry_data_list = entry_data_list->next;
      break;

    case MMDB_DATA_TYPE_DOUBLE:
      dump_geodata_into_msg_data(msg, path, "%f", entry_data_list->entry_data.double_value);
      entry_data_list = entry_data_list->next;
      break;

    case MMDB_DATA_TYPE_FLOAT:
      dump_geodata_into_msg_data(msg, path, "%f", (double)entry_data_list->entry_data.float_value);
      entry_data_list = entry_data_list->next;
      break;

    case MMDB_DATA_TYPE_UINT16:
      dump_geodata_into_msg_data(msg, path, "%u", entry_data_list->entry_data.uint16);
      entry_data_list = entry_data_list->next;
      break;

    case MMDB_DATA_TYPE_UINT32:
      dump_geodata_into_msg_data(msg, path, "%u", entry_data_list->entry_data.uint32);
      entry_data_list = entry_data_list->next;
      break;

    case MMDB_DATA_TYPE_UINT64:
      dump_geodata_into_msg_data(msg, path, "%" PRIu64, entry_data_list->entry_data.uint64);
      entry_data_list = entry_data_list->next;
      break;

    case MMDB_DATA_TYPE_INT32:
      dump_geodata_into_msg_data(msg, path, "%d", entry_data_list->entry_data.int32);
      entry_data_list = entry_data_list->next;
      break;
    case MMDB_DATA_TYPE_BOOLEAN:
      dump_geodata_into_msg_data(msg, path, "%s", entry_data_list->entry_data.boolean ? "true" : "false");
      entry_data_list = entry_data_list->next;
      break;
    default:
      *status = MMDB_INVALID_DATA_ERROR;
      return NULL;
    }

  *status = MMDB_SUCCESS;
  return entry_data_list;
}
