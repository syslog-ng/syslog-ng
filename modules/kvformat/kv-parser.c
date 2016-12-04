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
#include "kv-scanner-simple.h"
#include "kv-scanner-generic.h"

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
kv_parser_set_allow_pair_separator_in_value(LogParser *s, gboolean allow_pair_separator_in_value)
{
  KVParser *self = (KVParser *) s;

  self->allow_pair_separator_in_value = allow_pair_separator_in_value;
}

void
kv_parser_set_value_separator(LogParser *s, gchar value_separator)
{
  KVParser *self = (KVParser *) s;

  self->value_separator = value_separator;
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
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
         gsize input_len)
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

LogPipe *
kv_parser_clone_method(KVParser *dst, KVParser *src)
{
  kv_parser_set_prefix(&dst->super, src->prefix);
  log_parser_set_template(&dst->super, log_template_ref(src->super.template));

  kv_parser_set_allow_pair_separator_in_value(&dst->super, src->allow_pair_separator_in_value);
  kv_parser_set_value_separator(&dst->super, src->value_separator);

  if (src->kv_scanner)
    dst->kv_scanner = kv_scanner_clone(src->kv_scanner);

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

  kv_scanner_free(self->kv_scanner);
  g_string_free(self->formatted_key, TRUE);
  g_free(self->prefix);
  log_parser_free_method(s);
}

static gboolean
_process_threaded(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                  gsize input_len)
{
  LogParser *self = (LogParser *)log_pipe_clone(&s->super);

  gboolean ok = _process(self, pmsg, path_options, input, input_len);

  log_pipe_unref(&self->super);
  return ok;
}

gboolean
kv_parser_init_method(LogPipe *s)
{
  KVParser *self = (KVParser *)s;
  g_assert(self->kv_scanner == NULL);

  if (self->allow_pair_separator_in_value)
    {
      self->kv_scanner = kv_scanner_generic_new(self->value_separator, NULL);
    }
  else
    {
      self->kv_scanner = kv_scanner_simple_new(self->value_separator, NULL, FALSE);
    }

  return TRUE;
}

gboolean
kv_parser_deinit_method(LogPipe *s)
{
  KVParser *self = (KVParser *)s;

  kv_scanner_free(self->kv_scanner);
  self->kv_scanner = NULL;
  return TRUE;
}

KVParser *
kv_parser_init_instance(GlobalConfig *cfg)
{
  KVParser *self = g_new0(KVParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = kv_parser_init_method;
  self->super.super.deinit = kv_parser_deinit_method;
  self->super.super.free_fn = _free;
  self->super.process = _process_threaded;
  self->kv_scanner = NULL;
  self->value_separator = '=';
  self->allow_pair_separator_in_value = FALSE;
  self->formatted_key = g_string_sized_new(32);

  return self;
}

LogParser *
kv_parser_new(GlobalConfig *cfg)
{
  KVParser *self = kv_parser_init_instance(cfg);

  self->super.super.clone = _clone;

  return &self->super;
}
