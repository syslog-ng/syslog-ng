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
#include "str-repr/encode.h"

typedef struct
{
  LogMessage *msg;
  gboolean create_lists;
} PushParams;

XMLScannerOptions *
xml_parser_get_scanner_options(LogParser *p)
{
  XMLParser *self = (XMLParser *)p;
  return &self->options;
}

static void
remove_trailing_dot(gchar *str)
{
  if (!strlen(str))
    return;
  if (str[strlen(str)-1] == '.')
    str[strlen(str)-1] = 0;
}

static void
encode_and_append_value(GString *result, const gchar *current_value, gssize current_value_len)
{
  if (result->len > 0)
    g_string_append_c(result, ',');
  str_repr_encode_append(result, current_value, current_value_len, ",");
}

static GString *
append_values(const gchar *previous_value, gssize previous_value_len,
              const gchar *current_value, gssize current_value_len, gboolean create_lists)
{
  GString *result = scratch_buffers_alloc();
  g_string_append_len(result, previous_value, previous_value_len);
  if (create_lists)
    {
      encode_and_append_value(result, current_value, current_value_len);
    }
  else
    {
      g_string_append_len(result, current_value, current_value_len);
    }
  return result;
}

static void
scanner_push_function(const gchar *name, const gchar *value, gssize value_length, gpointer user_data)
{
  PushParams *push_params = (PushParams *) user_data;

  gssize current_value_len = 0;
  const gchar *current_value = log_msg_get_value_by_name(push_params->msg, name, &current_value_len);

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);
  GString *values_appended = append_values(current_value, current_value_len, value, value_length,
                                           push_params->create_lists);
  log_msg_set_value_by_name(push_params->msg, name, values_appended->str, values_appended->len);
  scratch_buffers_reclaim_marked(marker);
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

  PushParams push_params = {.msg = msg, .create_lists = self->create_lists};
  xml_scanner_init(&xml_scanner, &self->options, &scanner_push_function, &push_params, self->prefix);

  GError *error = NULL;
  xml_scanner_parse(&xml_scanner, input, input_len, &error);
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
xml_parser_set_forward_invalid(LogParser *s, gboolean setting)
{
  XMLParser *self = (XMLParser *) s;
  self->forward_invalid = setting;
}

void
xml_parser_allow_create_lists(LogParser *s, gboolean setting)
{
  XMLParser *self = (XMLParser *) s;
  self->create_lists = setting;
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
  xml_parser_allow_create_lists(&cloned->super, self->create_lists);
  xml_scanner_options_copy(&cloned->options, &self->options);

  return &cloned->super.super;
}

static void
xml_parser_free(LogPipe *s)
{
  XMLParser *self = (XMLParser *) s;
  g_free(self->prefix);
  self->prefix = NULL;
  xml_scanner_options_destroy(&self->options);
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
  self->create_lists = TRUE;
  if (cfg_is_config_version_older(cfg, VERSION_VALUE_3_20))
    {
      msg_warning_once("WARNING: xml-parser() introduced list-support in " VERSION_3_20 " version."
                       " If you would like to use the old functionality, use create-lists(no) option");
    }
  xml_parser_set_prefix(&self->super, ".xml");
  xml_scanner_options_defaults(&self->options);
  return &self->super;
}
