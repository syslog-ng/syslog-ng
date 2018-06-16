/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
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
#include "synthetic-message.h"
#include "pdb-error.h"
#include "template/templates.h"
#include "logmsg/logmsg.h"
#include "logpipe.h"

void
synthetic_message_set_inherit_mode(SyntheticMessage *self, SyntheticMessageInheritMode inherit_mode)
{
  self->inherit_mode = inherit_mode;
}

gboolean
synthetic_message_set_inherit_mode_string(SyntheticMessage *self, const gchar *inherit_mode_name, GError **error)
{
  gint inherit_mode = synthetic_message_lookup_inherit_mode(inherit_mode_name);

  if (inherit_mode < 0)
    {
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED, "Unknown inherit mode %s", inherit_mode_name);
      return FALSE;
    }
  synthetic_message_set_inherit_mode(self, inherit_mode);
  return TRUE;
}

void
synthetic_message_set_inherit_properties_string(SyntheticMessage *self, const gchar *inherit_properties, GError **error)
{
  SyntheticMessageInheritMode inherit_mode;

  if (strcasecmp(inherit_properties, "context") == 0)
    {
      inherit_mode = RAC_MSG_INHERIT_CONTEXT;
    }
  else if (inherit_properties[0] == 'T' || inherit_properties[0] == 't' ||
           inherit_properties[0] == '1')
    {
      inherit_mode = RAC_MSG_INHERIT_LAST_MESSAGE;
    }
  else if (inherit_properties[0] == 'F' || inherit_properties[0] == 'f' ||
           inherit_properties[0] == '0')
    {
      inherit_mode = RAC_MSG_INHERIT_NONE;
    }
  else
    {
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED, "Unknown inherit-properties: %s", inherit_properties);
      return;
    }
  synthetic_message_set_inherit_mode(self, inherit_mode);
}

void
synthetic_message_add_tag(SyntheticMessage *self, const gchar *text)
{
  LogTagId tag;

  if (!self->tags)
    self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));
  tag = log_tags_get_by_name(text);
  g_array_append_val(self->tags, tag);
}

gboolean
synthetic_message_add_value_template_string(SyntheticMessage *self, GlobalConfig *cfg, const gchar *name,
                                            const gchar *value, GError **error)
{
  LogTemplate *value_template;
  gboolean result = FALSE;

  /* NOTE: we shouldn't use the name property for LogTemplate structs, see the comment at log_template_set_name() */
  value_template = log_template_new(cfg, name);
  if (log_template_compile(value_template, value, error))
    {
      synthetic_message_add_value_template(self, name, value_template);
      result = TRUE;
    }
  log_template_unref(value_template);
  return result;
}

void
synthetic_message_add_value_template(SyntheticMessage *self, const gchar *name, LogTemplate *value)
{
  if (!self->values)
    self->values = g_ptr_array_new();

  /* NOTE: we shouldn't use the name property for LogTemplate structs, see the comment at log_template_set_name() */
  log_template_set_name(value, name);
  g_ptr_array_add(self->values, log_template_ref(value));
}

void
synthetic_message_apply(SyntheticMessage *self, CorrellationContext *context, LogMessage *msg, GString *buffer)
{
  gint i;

  if (self->tags)
    {
      for (i = 0; i < self->tags->len; i++)
        log_msg_set_tag_by_id(msg, g_array_index(self->tags, LogTagId, i));
    }

  if (self->values)
    {
      for (i = 0; i < self->values->len; i++)
        {
          log_template_format_with_context(g_ptr_array_index(self->values, i),
                                           context ? (LogMessage **) context->messages->pdata : &msg,
                                           context ? context->messages->len : 1,
                                           NULL, LTZ_LOCAL, 0, context ? context->key.session_id : NULL, buffer);
          log_msg_set_value_by_name(msg,
                                    ((LogTemplate *) g_ptr_array_index(self->values, i))->name,
                                    buffer->str,
                                    buffer->len);
        }
    }

}

static LogMessage *
_generate_message_inheriting_properties_from_the_last_message(LogMessage *msg)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  path_options.ack_needed = FALSE;
  return log_msg_clone_cow(msg, &path_options);
}

static LogMessage *
_generate_new_message_with_timestamp_of_the_triggering_message(LogStamp *msgstamp)
{
  LogMessage *genmsg = log_msg_new_local();

  genmsg->timestamps[LM_TS_STAMP] = *msgstamp;
  return genmsg;
}

LogMessage *
_generate_message_inheriting_properties_from_the_entire_context(CorrellationContext *context)
{
  LogMessage *genmsg = _generate_message_inheriting_properties_from_the_last_message(
                         correllation_context_get_last_message(context));

  log_msg_merge_context(genmsg, (LogMessage **) context->messages->pdata, context->messages->len);
  return genmsg;
}

static LogMessage *
_generate_default_message(SyntheticMessageInheritMode inherit_mode, LogMessage *triggering_msg)
{
  switch (inherit_mode)
    {
    case RAC_MSG_INHERIT_LAST_MESSAGE:
    case RAC_MSG_INHERIT_CONTEXT:
      return _generate_message_inheriting_properties_from_the_last_message(triggering_msg);
    case RAC_MSG_INHERIT_NONE:
      return _generate_new_message_with_timestamp_of_the_triggering_message(&triggering_msg->timestamps[LM_TS_STAMP]);
    default:
      g_assert_not_reached();
    }
}

static LogMessage *
_generate_default_message_from_context(SyntheticMessageInheritMode inherit_mode, CorrellationContext *context)
{
  LogMessage *triggering_msg = correllation_context_get_last_message(context);

  if (inherit_mode != RAC_MSG_INHERIT_CONTEXT)
    return _generate_default_message(inherit_mode, triggering_msg);

  return _generate_message_inheriting_properties_from_the_entire_context(context);
}

LogMessage *
synthetic_message_generate_with_context(SyntheticMessage *self, CorrellationContext *context, GString *buffer)
{
  LogMessage *genmsg;

  genmsg = _generate_default_message_from_context(self->inherit_mode, context);
  switch (context->key.scope)
    {
    case RCS_PROCESS:
      log_msg_set_value(genmsg, LM_V_PID, context->key.pid, -1);
    case RCS_PROGRAM:
      log_msg_set_value(genmsg, LM_V_PROGRAM, context->key.program, -1);
    case RCS_HOST:
      log_msg_set_value(genmsg, LM_V_HOST, context->key.host, -1);
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
  synthetic_message_apply(self, context, genmsg, buffer);
  return genmsg;
}

LogMessage *
synthetic_message_generate_without_context(SyntheticMessage *self, LogMessage *msg, GString *buffer)
{
  LogMessage *genmsg;

  genmsg = _generate_default_message(self->inherit_mode, msg);

  /* no context, which means no correllation. The action
   * rule contains the generated message at @0 and the one
   * which triggered the rule in @1.
   *
   * We emulate a context having only these two
   * messages, but without allocating a full-blown
   * structure.
   */
  LogMessage *dummy_msgs[] = { msg, NULL };
  GPtrArray dummy_ptr_array = { .pdata = (void **) dummy_msgs, .len = 2 };
  CorrellationContext dummy_context = { .messages = &dummy_ptr_array, 0 };

  synthetic_message_apply(self, &dummy_context, genmsg, buffer);
  return genmsg;
}

void
synthetic_message_init(SyntheticMessage *self)
{
  memset(self, 0, sizeof(*self));
}

void
synthetic_message_deinit(SyntheticMessage *self)
{
  gint i;

  if (self->tags)
    g_array_free(self->tags, TRUE);

  if (self->values)
    {
      for (i = 0; i < self->values->len; i++)
        log_template_unref(g_ptr_array_index(self->values, i));

      g_ptr_array_free(self->values, TRUE);
    }
}

SyntheticMessage *
synthetic_message_new(void)
{
  SyntheticMessage *self = g_new0(SyntheticMessage, 1);

  self->inherit_mode = RAC_MSG_INHERIT_CONTEXT;
  return self;
}

void
synthetic_message_free(SyntheticMessage *self)
{
  synthetic_message_deinit(self);
  g_free(self);
}

gint
synthetic_message_lookup_inherit_mode(const gchar *inherit_mode)
{
  if (strcasecmp(inherit_mode, "none") == 0)
    return RAC_MSG_INHERIT_NONE;
  else if (strcasecmp(inherit_mode, "last-message") == 0)
    return RAC_MSG_INHERIT_LAST_MESSAGE;
  else if (strcasecmp(inherit_mode, "context") == 0)
    return RAC_MSG_INHERIT_CONTEXT;
  return -1;
}
