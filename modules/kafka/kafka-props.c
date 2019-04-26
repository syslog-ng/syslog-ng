/*
 * Copyright (c) 2018 Kokan <kokaipeter@gmail.com>
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Balazs Scheidler
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
#include "kafka-props.h"
#include "messages.h"
#include "str-utils.h"
#include <stdio.h>
#include <stdlib.h>

KafkaProperty *
kafka_property_new(const gchar *name, const gchar *value)
{
  KafkaProperty *self = g_new0(KafkaProperty, 1);

  self->name = g_strdup(name);
  self->value = g_strdup(value);
  return self;
}

void
kafka_property_free(KafkaProperty *self)
{
  g_free(self->name);
  g_free(self->value);
  g_free(self);
}

void
kafka_property_list_free(GList *l)
{
  g_list_foreach(l, (GFunc) kafka_property_free, NULL);

  g_list_free(l);
}


typedef struct _KafkaPropertyFileReader
{
  FILE *fp;
  GString *line;
  GString *name;
  GString *value;
  gint line_number;
} KafkaPropertyFileReader;

static void
kafka_properties_file_reader_init(KafkaPropertyFileReader *self)
{
  self->name = g_string_sized_new(32);
  self->value = g_string_sized_new(128);
  self->line = g_string_sized_new(128);
  self->fp = NULL;
}

static void
kafka_properties_file_reader_destroy(KafkaPropertyFileReader *self)
{
  if (self->fp)
    fclose(self->fp);
  g_string_free(self->line, TRUE);
  g_string_free(self->name, TRUE);
  g_string_free(self->value, TRUE);
}

static gboolean
kafka_properties_file_reader_open(KafkaPropertyFileReader *self, const gchar *filename)
{
  if (!(self->fp = fopen(filename, "r")))
    {
      msg_error("Failed to open kafka properties file",
                evt_tag_str("file", filename),
                evt_tag_error("error"));
      return FALSE;
    }
  msg_debug("Reading kafka properties file",
            evt_tag_str("file", filename));
  return TRUE;
}

static const gboolean
kafka_properties_file_reader_get_line(KafkaPropertyFileReader *self)
{
  gchar buf[1024];

  if (!fgets(buf, sizeof(buf), self->fp))
    return FALSE;

  self->line_number++;

  /* Left-trim */
  gchar *start = buf;
  while (g_ascii_isspace(*start))
    start++;

  /* Right-trim and remove newline */
  gchar *end = start + strlen(start) - 1;
  while (end > start && g_ascii_isspace(*end))
    {
      *end = 0;
      end--;
    }
  g_string_append_len(self->line, start, end - start + 1);
  return TRUE;
}

static const gchar *
kafka_properties_file_reader_find_separator(KafkaPropertyFileReader *self)
{
  gboolean quoted = FALSE;

  for (const gchar *p = self->line->str; *p; p++)
    {
      if (!quoted)
        {
          if (*p == '\\')
            quoted = TRUE;
          else if (*p == ':' || *p == '=')
            return p;
        }
      else
        quoted = FALSE;
    }
  return NULL;
}

static gboolean
kafka_properties_file_reader_is_continued_on_next_line(KafkaPropertyFileReader *self)
{
  gboolean quoted = FALSE;

  for (const gchar *p = self->line->str; *p; p++)
    {
      if (!quoted && *p == '\\')
        quoted = TRUE;
      else
        quoted = FALSE;
    }
  /* if the end of line is quoted, we need to continue */
  if (quoted)
    return TRUE;
  return FALSE;
}

static gboolean
kafka_properties_file_reader_remove_continuation_indicator(KafkaPropertyFileReader *self)
{
  /* remove trailing '\' */
  g_string_truncate(self->line, self->line->len - 1);
  return TRUE;
}

static gchar *
kafka_properties_file_reader_deescape(KafkaPropertyFileReader *self, gsize start_pos, gsize end_pos)
{
  GString *decoded_value = g_string_sized_new(32);
  gboolean quoted = FALSE;

  for (const gchar *p = &self->line->str[start_pos]; p < &self->line->str[end_pos]; p++)
    {
      if (!quoted)
        {
          if (*p == '\\')
            quoted = TRUE;
          else
            g_string_append_c(decoded_value, *p);
        }
      else
        {
          quoted = FALSE;
          switch (*p)
            {
            case 'n':
              g_string_append_c(decoded_value, '\n');
              break;
            case 't':
              g_string_append_c(decoded_value, '\t');
              break;
            case 'r':
              g_string_append_c(decoded_value, '\r');
              break;
            case 'u':
              if (strlen(p + 1) >= 4)
                {
                  gchar unicode_hex[5];
                  gchar *end = NULL;

                  strncpy(unicode_hex, p+1, sizeof(unicode_hex));
                  unicode_hex[4] = 0;

                  gunichar uni = strtol(unicode_hex, &end, 16);
                  if (*end == 0)
                    g_string_append_unichar(decoded_value, uni);
                  p += 4;
                }
              break;
            default:
              g_string_append_c(decoded_value, *p);
            }
        }
    }
  while (decoded_value->len > 0 &&
    g_ascii_isspace(decoded_value->str[decoded_value->len - 1]))
    g_string_truncate(decoded_value, decoded_value->len - 1);
  return g_string_free(decoded_value, FALSE);
}

static gchar *
kafka_properties_file_reader_extract_key(KafkaPropertyFileReader *self, const gchar *separator)
{
  return kafka_properties_file_reader_deescape(self, 0, separator - self->line->str);
}

static gchar *
kafka_properties_file_reader_extract_value(KafkaPropertyFileReader *self, const gchar *separator)
{
  gsize value_offset = separator - self->line->str + 1;
  while (value_offset < self->line->len &&
         g_ascii_isspace(self->line->str[value_offset]))
    value_offset++;
  return kafka_properties_file_reader_deescape(self, value_offset, self->line->len);
}

static GList *
kafka_properties_file_reader_parse(KafkaPropertyFileReader *self)
{
  GList *prop_list = NULL;

  g_string_truncate(self->line, 0);
  while (kafka_properties_file_reader_get_line(self))
    {
      const gchar *line = self->line->str;

      if (!(line[0] == 0 || line[0] == '#' || line[0] == '!'))
        {

          while (kafka_properties_file_reader_is_continued_on_next_line(self) &&
                 kafka_properties_file_reader_remove_continuation_indicator(self) &&
                 kafka_properties_file_reader_get_line(self))
            ;
          const gchar *separator = kafka_properties_file_reader_find_separator(self);

          gchar *key = kafka_properties_file_reader_extract_key(self, separator);
          gchar *value = kafka_properties_file_reader_extract_value(self, separator);
          prop_list = g_list_prepend(prop_list, kafka_property_new(key, value));
          g_free(key);
          g_free(value);
        }
      g_string_truncate(self->line, 0);
    }
  return g_list_reverse(prop_list);
}

GList *
kafka_read_properties_file(const char *path)
{
  KafkaPropertyFileReader reader;
  GList *result = NULL;

  kafka_properties_file_reader_init(&reader);
  if (kafka_properties_file_reader_open(&reader, path))
    result = kafka_properties_file_reader_parse(&reader);
  kafka_properties_file_reader_destroy(&reader);
  return result;
}
