#include "synthetic-message.h"

#include "template/templates.h"
#include "tags.h"
#include "logmsg.h"
#include "logpipe.h"

void
synthetic_message_add_tag(PDBMessage *self, const gchar *text)
{
  LogTagId tag;

  if (!self->tags)
    self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));
  tag = log_tags_get_by_name(text);
  g_array_append_val(self->tags, tag);
}

gboolean
synthetic_message_add_value_template(PDBMessage *self, GlobalConfig *cfg, const gchar *name, const gchar *value, GError **error)
{
  LogTemplate *value_template;

  if (!self->values)
    self->values = g_ptr_array_new();

  value_template = log_template_new(cfg, name);
  if (!log_template_compile(value_template, value, error))
    {
      log_template_unref(value_template);
      return FALSE;
    }
  else
    g_ptr_array_add(self->values, value_template);
  return TRUE;
}

void
synthetic_message_apply(PDBMessage *self, CorrellationContext *context, LogMessage *msg, GString *buffer)
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
  LogMessage *genmsg;

  genmsg = log_msg_new_empty();
  genmsg->flags |= LF_LOCAL;
  genmsg->timestamps[LM_TS_STAMP] = *msgstamp;
  return genmsg;
}

LogMessage *
_generate_message_inheriting_properties_from_the_entire_context(CorrellationContext *context)
{
  LogMessage *genmsg = _generate_message_inheriting_properties_from_the_last_message(correllation_context_get_last_message(context));

  log_msg_merge_context(genmsg, (LogMessage **) context->messages->pdata, context->messages->len);
  return genmsg;
}

static LogMessage *
_generate_default_message(gint inherit_mode, LogMessage *triggering_msg)
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
_generate_default_message_from_context(gint inherit_mode, CorrellationContext *context)
{
  LogMessage *triggering_msg = correllation_context_get_last_message(context);

  if (inherit_mode != RAC_MSG_INHERIT_CONTEXT)
    return _generate_default_message(inherit_mode, triggering_msg);

  return _generate_message_inheriting_properties_from_the_entire_context(context);
}

LogMessage *
synthetic_message_generate_with_context(PDBMessage *self, gint inherit_mode, CorrellationContext *context, GString *buffer)
{
  LogMessage *genmsg;

  genmsg = _generate_default_message_from_context(inherit_mode, context);
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
  synthetic_message_apply(self, context, genmsg, buffer);
  g_ptr_array_remove_index_fast(context->messages, context->messages->len - 1);
  return genmsg;
}

LogMessage *
synthetic_message_generate_without_context(PDBMessage *self, gint inherit_mode, LogMessage *msg, GString *buffer)
{
  LogMessage *genmsg;

  genmsg = _generate_default_message(inherit_mode, msg);

  /* no context, which means no correllation. The action
   * rule contains the generated message at @0 and the one
   * which triggered the rule in @1.
   *
   * We emulate a context having only these two
   * messages, but without allocating a full-blown
   * structure.
   */
  LogMessage *dummy_msgs[] = { msg, genmsg, NULL };
  GPtrArray dummy_ptr_array = { .pdata = (void **) dummy_msgs, .len = 2 };
  CorrellationContext dummy_context = { .messages = &dummy_ptr_array, 0 };

  synthetic_message_apply(self, &dummy_context, genmsg, buffer);
  return genmsg;
}

void
synthetic_message_deinit(PDBMessage *self)
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

void
synthetic_message_free(PDBMessage *self)
{
  synthetic_message_deinit(self);
  g_free(self);
}
