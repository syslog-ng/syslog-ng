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

#include "xml.h"
#include "scratch-buffers.h"
#include "xml-scanner.h"


static void
remove_trailing_dot(gchar *str)
{
  g_assert(strlen(str));
  if (str[strlen(str)-1] == '.')
    str[strlen(str)-1] = 0;
}

static gboolean
xml_parser_process(LogParser *s, LogMessage **pmsg,
                   const LogPathOptions *path_options,
                   const gchar *input, gsize input_len)
{
  XMLParser *self = (XMLParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  XMLScanner xml_scanner;
  msg_trace("xml-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_str ("prefix", self->prefix),
            evt_tag_printf("msg", "%p", *pmsg));

  GString *key = scratch_buffers_alloc();
  key = g_string_append(key, self->prefix);

  InserterState state = { .msg = msg, .key = key, .parser = self };
  xml_scanner_init(&xml_scanner, &state);

  GError *error = NULL;
  xml_scanner_parse(&xml_scanner, input, input_len, &error);
  if (error)
    goto err;

  xml_scanner_end_parse(&xml_scanner, &error);
  if (error)
    goto err;

  xml_scanner_deinit(&xml_scanner);
  return TRUE;

err:
  msg_error("xml-parser failed",
            evt_tag_str("error", error->message),
            evt_tag_int("forward_invalid", self->forward_invalid));
  g_error_free(error);
  xml_scanner_deinit(&xml_scanner);
  return self->forward_invalid;
}

void
xml_parser_set_exclude_tags(LogParser *s, GList *exclude_tags)
{
  XMLParser *self = (XMLParser *) s;

  g_list_free_full(self->exclude_tags, g_free);
  self->exclude_tags = g_list_copy_deep(exclude_tags, ((GCopyFunc)g_strdup), NULL);
}

void
xml_parser_set_forward_invalid(LogParser *s, gboolean setting)
{
  XMLParser *self = (XMLParser *) s;
  self->forward_invalid = setting;
}

void
xml_parser_set_strip_whitespaces(LogParser *s, gboolean setting)
{
  XMLParser *self = (XMLParser *) s;
  self->strip_whitespaces = setting;
}

void
xml_parser_set_prefix(LogParser *s, const gchar *prefix)
{
  XMLParser *self = (XMLParser *) s;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

LogPipe *
xml_parser_clone(LogPipe *s)
{
  XMLParser *self = (XMLParser *) s;
  XMLParser *cloned;

  cloned = (XMLParser *) xml_parser_new(s->cfg);

  xml_parser_set_prefix(&cloned->super, self->prefix);
  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));
  xml_parser_set_forward_invalid(&cloned->super, self->forward_invalid);
  xml_parser_set_strip_whitespaces(&cloned->super, self->strip_whitespaces);
  xml_parser_set_exclude_tags(&cloned->super, self->exclude_tags);

  return &cloned->super.super;
}

static void
xml_parser_free(LogPipe *s)
{
  XMLParser *self = (XMLParser *) s;
  g_free(self->prefix);
  self->prefix = NULL;
  g_list_free_full(self->exclude_tags, g_free);
  self->exclude_tags = NULL;

  log_parser_free_method(s);
}


static gboolean
xml_parser_init(LogPipe *s)
{
  XMLParser *self = (XMLParser *)s;
  remove_trailing_dot(self->prefix);
  return log_parser_init_method(s);
}


LogParser *
xml_parser_new(GlobalConfig *cfg)
{
  XMLParser *self = g_new0(XMLParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = xml_parser_init;
  self->super.super.free_fn = xml_parser_free;
  self->super.super.clone = xml_parser_clone;
  self->super.process = xml_parser_process;
  self->forward_invalid = TRUE;

  xml_parser_set_prefix(&self->super, ".xml");
  return &self->super;
}
