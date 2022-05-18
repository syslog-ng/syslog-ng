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
#include "scanner/kv-scanner/kv-scanner.h"
#include "scratch-buffers.h"

gboolean
kv_parser_is_valid_separator_character(char c)
{
  return (c != ' '  &&
          c != '\'' &&
          c != '\"' );
}

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

  self->value_separator = value_separator;
}

void
kv_parser_set_pair_separator(LogParser *s, const gchar *pair_separator)
{
  KVParser *self = (KVParser *) s;

  g_free(self->pair_separator);
  self->pair_separator = g_strdup(pair_separator);
}

void
kv_parser_set_stray_words_value_name(LogParser *s, const gchar *value_name)
{
  KVParser *self = (KVParser *) s;

  g_free(self->stray_words_value_name);
  self->stray_words_value_name = g_strdup(value_name);
}

static const gchar *
_get_formatted_key_with_prefix(KVParser *self, const gchar *key, GString *formatted_key)
{
  if (formatted_key->len > 0)
    g_string_truncate(formatted_key, self->prefix_len);
  else
    g_string_assign(formatted_key, self->prefix);
  g_string_append(formatted_key, key);
  return formatted_key->str;
}

static const gchar *
_get_formatted_key(KVParser *self, const gchar *key, GString *formatted_key)
{
  if (!self->prefix)
    return key;
  return _get_formatted_key_with_prefix(self, key, formatted_key);
}

void
kv_parser_init_scanner_method(KVParser *self, KVScanner *kv_scanner)
{
  kv_scanner_init(kv_scanner, self->value_separator, self->pair_separator, self->stray_words_value_name != NULL);
}

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
         gsize input_len)
{
  KVParser *self = (KVParser *) s;
  KVScanner kv_scanner;
  kv_parser_init_scanner(self, &kv_scanner);
  GString *formatted_key = scratch_buffers_alloc();

  log_msg_make_writable(pmsg, path_options);
  msg_trace("kv-parser message processing started",
            evt_tag_str("input", input),
            evt_tag_str("prefix", self->prefix),
            evt_tag_msg_reference(*pmsg));
  /* FIXME: input length */
  kv_scanner_input(&kv_scanner, input);
  while (kv_scanner_scan_next(&kv_scanner))
    {

      /* FIXME: value length */
      log_msg_set_value_by_name(*pmsg,
                                _get_formatted_key(self, kv_scanner_get_current_key(&kv_scanner), formatted_key),
                                kv_scanner_get_current_value(&kv_scanner), -1);
    }
  if (self->stray_words_value_name)
    log_msg_set_value_by_name(*pmsg,
                              self->stray_words_value_name,
                              kv_scanner_get_stray_words(&kv_scanner), -1);

  kv_scanner_deinit(&kv_scanner);
  return TRUE;
}

LogPipe *
kv_parser_clone_method(KVParser *dst, KVParser *src)
{
  kv_parser_set_prefix(&dst->super, src->prefix);
  log_parser_set_template(&dst->super, log_template_ref(src->super.template));
  kv_parser_set_value_separator(&dst->super, src->value_separator);
  kv_parser_set_pair_separator(&dst->super, src->pair_separator);
  kv_parser_set_stray_words_value_name(&dst->super, src->stray_words_value_name);

  return &dst->super.super;
}

static LogPipe *
_clone(LogPipe *s)
{
  KVParser *self = (KVParser *) s;
  KVParser *cloned = (KVParser *) kv_parser_new(s->cfg);

  return kv_parser_clone_method(cloned, self);
}

static void
_free(LogPipe *s)
{
  KVParser *self = (KVParser *)s;

  g_free(self->prefix);
  g_free(self->pair_separator);
  g_free(self->stray_words_value_name);
  log_parser_free_method(s);
}

void
kv_parser_init_instance(KVParser *self, GlobalConfig *cfg)
{
  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = _free;
  self->super.process = _process;
  self->init_scanner = kv_parser_init_scanner_method;
  self->value_separator = '=';
  self->pair_separator = g_strdup(", ");
}

LogParser *
kv_parser_new(GlobalConfig *cfg)
{
  KVParser *self = g_new0(KVParser, 1);

  kv_parser_init_instance(self, cfg);
  self->super.super.clone = _clone;

  return &self->super;
}
