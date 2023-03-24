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
#include "scratch-buffers.h"

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
synthetic_message_add_value_template_string_and_type(SyntheticMessage *self, GlobalConfig *cfg, const gchar *name,
                                                     const gchar *value, const gchar *type_hint, GError **error)
{
  LogTemplate *value_template;
  gboolean success = FALSE;

  value_template = log_template_new(cfg, NULL);
  success = log_template_compile(value_template, value, error);
  if (success && type_hint)
    success = log_template_set_type_hint(value_template, type_hint, error);
  if (success)
    synthetic_message_add_value_template(self, name, value_template);

  log_template_unref(value_template);
  return success;
}

gboolean
synthetic_message_add_value_template_string(SyntheticMessage *self, GlobalConfig *cfg, const gchar *name,
                                            const gchar *value, GError **error)
{
  LogTemplate *value_template;
  gboolean success = FALSE;

  value_template = log_template_new(cfg, NULL);
  if (cfg_is_typing_feature_enabled(cfg))
    {
      /* typing is enabled as a feature, check @version */

      if (cfg_is_config_version_older(cfg, VERSION_VALUE_4_0))
        {
          /* old @version, let's use compat mode but warn on changes */
          if (strchr(value, '(') != NULL)
            {
              success = log_template_compile_with_type_hint(value_template, value, error);
              if (!success)
                {
                  log_template_set_type_hint(value_template, "string", NULL);

                  msg_warning("WARNING: the template specified in value()/<value> options for your grouping-by() or "
                              "db-parser() configuration has been changed to support typing from "
                              "" FEATURE_TYPING_VERSION ". You are using an older config "
                              "version and your template contains an unrecognized type-cast, probably a "
                              "parenthesis in the value field. This will be interpreted in the `type(value)' "
                              "format in future versions. Please add an "
                              "explicit string() cast as shown in the 'fixed-value' tag of this log message "
                              "or remove the parenthesis. The value will be processed as a 'string' "
                              "expression",
                              cfg_format_config_version_tag(cfg),
                              evt_tag_str("name", name),
                              evt_tag_str("value", value),
                              evt_tag_printf("fixed-value", "string(%s)", value));
                  g_clear_error(error);
                  success = log_template_compile(value_template, value, error);
                }
            }
          else
            {
              success = log_template_compile(value_template, value, error);
            }
        }
      else
        {
          /* recent @version */
          success = log_template_compile_with_type_hint(value_template, value, error);
        }
    }
  else
    {
      /* typing is not enabled use compat mode */
      success = log_template_compile(value_template, value, error);
    }

  if (success)
    synthetic_message_add_value_template(self, name, value_template);

  log_template_unref(value_template);
  return success;
}

void
synthetic_message_add_value_template(SyntheticMessage *self, const gchar *name, LogTemplate *value_template)
{
  if (!self->values)
    self->values = g_array_new(FALSE, FALSE, sizeof(SyntheticMessageValue));

  SyntheticMessageValue smv =
  {
    .name = g_strdup(name),
    .handle = 0,
    .value_template = log_template_ref(value_template)
  };
  g_array_append_val(self->values, smv);
}

void
synthetic_message_set_prefix(SyntheticMessage *self, const gchar *prefix)
{
  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

static NVHandle
_allocate_and_get_value_handle(SyntheticMessage *self, SyntheticMessageValue *smv)
{
  if (smv->handle)
    return smv->handle;

  if (!self->prefix)
    {
      smv->handle = log_msg_get_value_handle(smv->name);
      return smv->handle;
    }

  gchar *prefixed_name = g_strdup_printf("%s%s", self->prefix, smv->name);
  smv->handle = log_msg_get_value_handle(prefixed_name);
  g_free(prefixed_name);
  return smv->handle;
}

void
synthetic_message_apply(SyntheticMessage *self, CorrelationContext *context, LogMessage *msg)
{
  gint i;

  if (self->tags)
    {
      for (i = 0; i < self->tags->len; i++)
        log_msg_set_tag_by_id(msg, synthetic_message_tags_index(self, i));
    }

  if (self->values)
    {
      ScratchBuffersMarker marker;
      GString *buffer = scratch_buffers_alloc_and_mark(&marker);
      for (i = 0; i < self->values->len; i++)
        {
          LogMessageValueType type;
          LogTemplateEvalOptions options = {NULL, LTZ_LOCAL, 0, context ? context->key.session_id : NULL, LM_VT_STRING};
          SyntheticMessageValue *smv = synthetic_message_values_index(self, i);

          log_template_format_value_and_type_with_context(smv->value_template,
                                                          context ? (LogMessage **) context->messages->pdata : &msg,
                                                          context ? context->messages->len : 1,
                                                          &options, buffer, &type);
          log_msg_set_value_with_type(msg,
                                      _allocate_and_get_value_handle(self, smv),
                                      buffer->str, buffer->len, type);
        }
      scratch_buffers_reclaim_marked(marker);
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
_generate_new_message_with_timestamp_of_the_triggering_message(UnixTime *msgstamp)
{
  LogMessage *genmsg = log_msg_new_local();

  genmsg->timestamps[LM_TS_STAMP] = *msgstamp;
  return genmsg;
}

LogMessage *
_generate_message_inheriting_properties_from_the_entire_context(CorrelationContext *context)
{
  LogMessage *genmsg = _generate_message_inheriting_properties_from_the_last_message(
                         correlation_context_get_last_message(context));

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
_generate_default_message_from_context(SyntheticMessageInheritMode inherit_mode, CorrelationContext *context)
{
  LogMessage *triggering_msg = correlation_context_get_last_message(context);

  if (inherit_mode != RAC_MSG_INHERIT_CONTEXT)
    return _generate_default_message(inherit_mode, triggering_msg);

  return _generate_message_inheriting_properties_from_the_entire_context(context);
}

LogMessage *
synthetic_message_generate_with_context(SyntheticMessage *self, CorrelationContext *context)
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
  g_ptr_array_add(context->messages, genmsg);
  synthetic_message_apply(self, context, genmsg);
  g_ptr_array_remove_index_fast(context->messages, context->messages->len - 1);
  return genmsg;
}

LogMessage *
synthetic_message_generate_without_context(SyntheticMessage *self, LogMessage *msg)
{
  LogMessage *genmsg;

  genmsg = _generate_default_message(self->inherit_mode, msg);

  /* no context, which means no correlation. The action
   * rule contains the generated message at @0 and the one
   * which triggered the rule in @1.
   *
   * We emulate a context having only these two
   * messages, but without allocating a full-blown
   * structure.
   */
  LogMessage *dummy_msgs[] = { msg, genmsg, NULL };
  GPtrArray dummy_ptr_array = { .pdata = (void **) dummy_msgs, .len = 2 };
  CorrelationContext dummy_context = { .messages = &dummy_ptr_array, 0 };

  synthetic_message_apply(self, &dummy_context, genmsg);
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
        {
          SyntheticMessageValue *smv = synthetic_message_values_index(self, i);
          g_free(smv->name);
          log_template_unref(smv->value_template);
        }

      g_array_free(self->values, TRUE);
    }
  g_free(self->prefix);
}

SyntheticMessage *
synthetic_message_new(void)
{
  SyntheticMessage *self = g_new(SyntheticMessage, 1);

  synthetic_message_init(self);
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
