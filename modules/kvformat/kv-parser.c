/*
 * Copyright (c) 2015 Balabit
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
 */

#include "kv-parser.h"
#include "kv-scanner.h"

typedef struct _KVParser
{
  LogParser super;
  gchar *prefix;
  gsize prefix_len;
  GString *formatted_key;
  KVScanner *kv_scanner;
} KVParser;

void
kv_parser_set_prefix(LogParser *p, const gchar *prefix)
{
  KVParser *self = (KVParser *)p;

  g_free(self->prefix);
  if (prefix)
    {
      self->prefix = g_strdup(prefix);
      self->prefix_len = strlen(prefix);
    }
  else
    {
      self->prefix = NULL;
      self->prefix_len = 0;
    }
}

void
kv_parser_set_value_separator(LogParser *s, gchar value_separator)
{
  KVParser *self = (KVParser *) s;

  kv_scanner_set_value_separator(self->kv_scanner, value_separator);
}

static const gchar *
_get_formatted_key(KVParser *self, const gchar *key)
{
  if (!self->prefix)
    return key;
  else if (self->formatted_key->len > 0)
    g_string_truncate(self->formatted_key, self->prefix_len);
  else
    g_string_assign(self->formatted_key, self->prefix);
  g_string_append(self->formatted_key, key);
  return self->formatted_key->str;
}

static gboolean
kv_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  KVParser *self = (KVParser *) s;

  log_msg_make_writable(pmsg, path_options);
  /* FIXME: input length */
  kv_scanner_input(self->kv_scanner, input);
  while (kv_scanner_scan_next(self->kv_scanner))
    {

      /* FIXME: value length */
      log_msg_set_value_by_name(*pmsg,
                                _get_formatted_key(self, kv_scanner_get_current_key(self->kv_scanner)),
                                kv_scanner_get_current_value(self->kv_scanner), -1);
    }
  return TRUE;
}

static LogPipe *
kv_parser_clone(LogPipe *s)
{
  KVParser *self = (KVParser *) s;
  LogParser *cloned;

  cloned = kv_parser_new(s->cfg, kv_scanner_clone(self->kv_scanner));
  kv_parser_set_prefix(cloned, self->prefix);

  return &cloned->super;
}

static void
kv_parser_free(LogPipe *s)
{
  KVParser *self = (KVParser *)s;

  kv_scanner_free(self->kv_scanner);
  g_string_free(self->formatted_key, TRUE);
  g_free(self->prefix);
  log_parser_free_method(s);
}

LogParser *
kv_parser_new(GlobalConfig *cfg, KVScanner *kv_scanner)
{
  KVParser *self = g_new0(KVParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = kv_parser_free;
  self->super.super.clone = kv_parser_clone;
  self->super.process = kv_parser_process;

  self->kv_scanner = kv_scanner;
  self->formatted_key = g_string_sized_new(32);
  return &self->super;
}
