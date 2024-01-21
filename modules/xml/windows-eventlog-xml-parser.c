/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2024 Attila Szakacs
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

#include "windows-eventlog-xml-parser.h"
#include "xml-private.h"
#include "scratch-buffers.h"
#include "str-repr/encode.h"

typedef struct
{
  LogMessage *msg;
  gboolean create_lists;
  const gchar *prefix;
} PushParams;

static void
scanner_push_function(const gchar *name, const gchar *value, gssize value_length,
                      gpointer user_data)
{
  PushParams *push_params = (PushParams *) user_data;

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  gssize prefix_len = strlen(push_params->prefix);
  if (strncmp(push_params->prefix, name, prefix_len) == 0 && strcmp(".Event.EventData.Data", name + prefix_len) == 0)
    {
      GString *data_name_key_buffer = scratch_buffers_alloc();

      g_string_append(data_name_key_buffer, push_params->prefix);
      g_string_append(data_name_key_buffer, ".Event.EventData.Data.");
      gssize data_name_key_prefix_len = data_name_key_buffer->len;
      g_string_append(data_name_key_buffer, "_Name");

      NVHandle orig_data_name_handle = log_msg_get_value_handle(data_name_key_buffer->str);

      gssize data_name_len = 0;
      const gchar *data_name = log_msg_get_value(push_params->msg, orig_data_name_handle, &data_name_len);
      if (data_name_len != 0)
        {
          g_string_truncate(data_name_key_buffer, data_name_key_prefix_len);
          g_string_append_len(data_name_key_buffer, data_name, data_name_len);

          log_msg_set_value_by_name_with_type(push_params->msg, data_name_key_buffer->str, value, value_length,
                                              LM_VT_STRING);
          log_msg_unset_value(push_params->msg, orig_data_name_handle);

          scratch_buffers_reclaim_marked(marker);
          return;
        }
    }

  gssize current_value_len = 0;
  const gchar *current_value = log_msg_get_value_by_name(push_params->msg, name, &current_value_len);

  LogMessageValueType type;
  GString *values_appended = xml_parser_append_values(current_value, current_value_len, value, value_length,
                                                      push_params->create_lists, &type);
  log_msg_set_value_by_name_with_type(push_params->msg, name, values_appended->str, values_appended->len, type);
  scratch_buffers_reclaim_marked(marker);
}

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  XMLParser *self = (XMLParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  XMLScanner xml_scanner;
  msg_trace("windows-eventlog-xml-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_str ("prefix", self->prefix),
            evt_tag_msg_reference(*pmsg));

  PushParams push_params = {.msg = msg, .create_lists = self->create_lists, .prefix = self->prefix};
  xml_scanner_init(&xml_scanner, &self->options, &scanner_push_function, &push_params, self->prefix);

  GError *error = NULL;
  xml_scanner_parse(&xml_scanner, input, input_len, &error);
  if (error)
    goto err;

  xml_scanner_deinit(&xml_scanner);
  return TRUE;

err:
  msg_error("windows-eventlog-xml-parser failed",
            evt_tag_str("error", error->message),
            evt_tag_int("forward_invalid", self->forward_invalid));
  g_error_free(error);
  xml_scanner_deinit(&xml_scanner);
  return self->forward_invalid;
}

LogParser *
windows_eventlog_xml_parser_new(GlobalConfig *cfg)
{
  LogParser *xml_parser = xml_parser_new(cfg);

  xml_parser->process = _process;

  return xml_parser;
}

